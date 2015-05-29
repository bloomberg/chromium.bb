# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import datetime
import email.utils
import hashlib
import itertools
import json
import logging
import math
import os
import os.path
import random
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import urlparse

import SimpleHTTPServer
import SocketServer

from .paths import Paths

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                             os.pardir, 'build', 'android'))
from pylib import constants
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.utils import apk_helper

# Tags used by the mojo shell application logs.
LOGCAT_TAGS = [
    'AndroidHandler',
    'MojoFileHelper',
    'MojoMain',
    'MojoShellActivity',
    'MojoShellApplication',
    'chromium',
]

MAPPING_PREFIX = '--map-origin='

ZERO = datetime.timedelta(0)

class UTC_TZINFO(datetime.tzinfo):
  """UTC time zone representation."""

  def utcoffset(self, _):
    return ZERO

  def tzname(self, _):
    return "UTC"

  def dst(self, _):
     return ZERO

UTC = UTC_TZINFO()


class _SilentTCPServer(SocketServer.TCPServer):
  """
  A TCPServer that won't display any error, unless debugging is enabled. This is
  useful because the client might stop while it is fetching an URL, which causes
  spurious error messages.
  """
  def handle_error(self, request, client_address):
    """Override the base class method to have conditional logging."""
    if logging.getLogger().isEnabledFor(logging.DEBUG):
      SocketServer.TCPServer.handle_error(self, request, client_address)


def _GetHandlerClassForPath(base_path):
  class RequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    """
    Handler for SocketServer.TCPServer that will serve the files from
    |base_path| directory over http.
    """

    def __init__(self, *args, **kwargs):
      self.etag = None
      SimpleHTTPServer.SimpleHTTPRequestHandler.__init__(self, *args, **kwargs)

    def get_etag(self):
      if self.etag:
        return self.etag

      path = self.translate_path(self.path)
      if not os.path.isfile(path):
        return None

      sha256 = hashlib.sha256()
      BLOCKSIZE = 65536
      with open(path, 'rb') as hashed:
        buf = hashed.read(BLOCKSIZE)
        while len(buf) > 0:
          sha256.update(buf)
          buf = hashed.read(BLOCKSIZE)
      self.etag = '"%s"' % sha256.hexdigest()
      return self.etag

    def send_head(self):
      # Always close the connection after each request, as the keep alive
      # support from SimpleHTTPServer doesn't like when the client requests to
      # close the connection before downloading the full response content.
      # pylint: disable=W0201
      self.close_connection = 1

      path = self.translate_path(self.path)
      if os.path.isfile(path):
        # Handle If-None-Match
        etag = self.get_etag()
        if ('If-None-Match' in self.headers and
            etag == self.headers['If-None-Match']):
          self.send_response(304)
          return None

        # Handle If-Modified-Since
        if ('If-None-Match' not in self.headers and
            'If-Modified-Since' in self.headers):
          last_modified = datetime.datetime.fromtimestamp(
              math.floor(os.stat(path).st_mtime), tz=UTC)
          ims = datetime.datetime(
              *email.utils.parsedate(self.headers['If-Modified-Since'])[:6],
              tzinfo=UTC)
          if last_modified <= ims:
            self.send_response(304)
            return None

      return SimpleHTTPServer.SimpleHTTPRequestHandler.send_head(self)

    def end_headers(self):
      path = self.translate_path(self.path)

      if os.path.isfile(path):
        etag = self.get_etag()
        if etag:
          self.send_header('ETag', etag)
          self.send_header('Cache-Control', 'must-revalidate')

      return SimpleHTTPServer.SimpleHTTPRequestHandler.end_headers(self)

    def translate_path(self, path):
      path_from_current = (
          SimpleHTTPServer.SimpleHTTPRequestHandler.translate_path(self, path))
      return os.path.join(base_path, os.path.relpath(path_from_current))

    def log_message(self, *_):
      """Override the base class method to disable logging."""
      pass

  RequestHandler.protocol_version = 'HTTP/1.1'
  return RequestHandler


def _IsMapOrigin(arg):
  """Returns whether arg is a --map-origin argument."""
  return arg.startswith(MAPPING_PREFIX)


def _Split(l, pred):
  positive = []
  negative = []
  for v in l:
    if pred(v):
      positive.append(v)
    else:
      negative.append(v)
  return (positive, negative)


def _ExitIfNeeded(process):
  """Exits |process| if it is still alive."""
  if process.poll() is None:
    process.kill()


class AndroidShell(object):
  """
  Used to set up and run a given mojo shell binary on an Android device.

  Args:
    config: The mopy.config.Config for the build.
    target_device: device to run on, if multiple devices are connected
  """
  def __init__(self, config, target_device=None):
    self.adb_path = constants.GetAdbPath()
    self.paths = Paths(config)
    self.target_device = target_device
    self.target_package = apk_helper.GetPackageName(self.paths.apk_path)
    # This is used by decive_utils.Install to check if the apk needs updating.
    constants.SetOutputDirectory(self.paths.build_dir)

  # TODO(msw): Use pylib's adb_wrapper and device_utils instead.
  def _CreateADBCommand(self, args):
    adb_command = [self.adb_path]
    if self.target_device:
      adb_command.extend(['-s', self.target_device])
    adb_command.extend(args)
    logging.getLogger().debug("Command: %s", " ".join(adb_command))
    return adb_command

  def _ReadFifo(self, fifo_path, pipe, on_fifo_closed, max_attempts=5):
    """
    Reads |fifo_path| on the device and write the contents to |pipe|. Calls
    |on_fifo_closed| when the fifo is closed. This method will try to find the
    path up to |max_attempts|, waiting 1 second between each attempt. If it
    cannot find |fifo_path|, a exception will be raised.
    """
    fifo_command = self._CreateADBCommand(
        ['shell', 'test -e "%s"; echo $?' % fifo_path])

    def Run():
      def _WaitForFifo():
        for _ in xrange(max_attempts):
          if subprocess.check_output(fifo_command)[0] == '0':
            return
          time.sleep(1)
        if on_fifo_closed:
          on_fifo_closed()
        raise Exception("Unable to find fifo.")
      _WaitForFifo()
      stdout_cat = subprocess.Popen(self._CreateADBCommand([
                                     'shell',
                                     'cat',
                                     fifo_path]),
                                    stdout=pipe)
      atexit.register(_ExitIfNeeded, stdout_cat)
      stdout_cat.wait()
      if on_fifo_closed:
        on_fifo_closed()

    thread = threading.Thread(target=Run, name="StdoutRedirector")
    thread.start()

  def _MapPort(self, device_port, host_port):
    """
    Maps the device port to the host port. If |device_port| is 0, a random
    available port is chosen. Returns the device port.
    """
    def _FindAvailablePortOnDevice():
      opened = subprocess.check_output(
          self._CreateADBCommand(['shell', 'netstat']))
      opened = [int(x.strip().split()[3].split(':')[1])
                for x in opened if x.startswith(' tcp')]
      while True:
        port = random.randint(4096, 16384)
        if port not in opened:
          return port
    if device_port == 0:
      device_port = _FindAvailablePortOnDevice()
    subprocess.Popen(self._CreateADBCommand([
                      "reverse",
                      "tcp:%d" % device_port,
                      "tcp:%d" % host_port])).wait()

    unmap_command = self._CreateADBCommand(["reverse", "--remove",
                     "tcp:%d" % device_port])

    def _UnmapPort():
      subprocess.Popen(unmap_command)
    atexit.register(_UnmapPort)
    return device_port

  def _StartHttpServerForDirectory(self, path, port=0):
    """Starts an http server serving from |path|. Returns the local URL."""
    assert path
    httpd = _SilentTCPServer(('127.0.0.1', 0), _GetHandlerClassForPath(path))
    atexit.register(httpd.shutdown)

    http_thread = threading.Thread(target=httpd.serve_forever)
    http_thread.daemon = True
    http_thread.start()

    print 'Hosting %s at http://127.0.0.1:%d' % (path, httpd.server_address[1])
    return 'http://127.0.0.1:%d/' % self._MapPort(port, httpd.server_address[1])

  def _StartHttpServerForOriginMapping(self, mapping, port):
    """
    If |mapping| points at a local file starts an http server to serve files
    from the directory and returns the new mapping. This is intended to be
    called for every --map-origin value.
    """
    parts = mapping.split('=')
    if len(parts) != 2:
      return mapping
    dest = parts[1]
    # If the destination is a URL, don't map it.
    if urlparse.urlparse(dest)[0]:
      return mapping
    # Assume the destination is a local file. Start a local server that
    # redirects to it.
    localUrl = self._StartHttpServerForDirectory(dest, port)
    print 'started server at %s for %s' % (dest, localUrl)
    return parts[0] + '=' + localUrl

  def _StartHttpServerForOriginMappings(self, map_parameters):
    """Calls _StartHttpServerForOriginMapping for every --map-origin arg."""
    if not map_parameters:
      return []

    original_values = list(itertools.chain(
        *map(lambda x: x[len(MAPPING_PREFIX):].split(','), map_parameters)))
    sorted(original_values)
    result = []
    for value in original_values:
      result.append(self._StartHttpServerForOriginMapping(value, 0))
    return [MAPPING_PREFIX + ','.join(result)]

  def PrepareShellRun(self, origin=None, gdb=False):
    """
    Prepares for StartShell: runs adb as root and installs the apk as needed.
    If the origin specified is 'localhost', a local http server will be set up
    to serve files from the build directory along with port forwarding.

    Returns arguments that should be appended to shell argument list.
    """
    devices = device_utils.DeviceUtils.HealthyDevices()
    if self.target_device:
      device = next((d for d in devices if d == self.target_device), None)
      if not device:
        raise device_errors.DeviceUnreachableError(self.target_device)
    elif devices:
      device = devices[0]
    else:
      raise device_errors.NoDevicesError()
    self.target_device = device.adb.GetDeviceSerial()

    logging.getLogger().debug("Using device: %s", device)
    device.EnableRoot()
    device.Install(self.paths.apk_path)

    atexit.register(self.StopShell)

    extra_args = []
    if origin is 'localhost':
      origin = self._StartHttpServerForDirectory(self.paths.build_dir, 0)
    if origin:
      extra_args.append("--origin=" + origin)

    if gdb:
      # Remote debugging needs a port forwarded.
      subprocess.check_call(self._CreateADBCommand(['forward', 'tcp:5039',
                                                    'tcp:5039']))

    return extra_args

  def _GetProcessId(self, process):
    """Returns the process id of the process on the remote device."""
    while True:
      line = process.stdout.readline()
      pid_command = 'launcher waiting for GDB. pid: '
      index = line.find(pid_command)
      if index != -1:
        return line[index + len(pid_command):].strip()
    return 0

  def _GetLocalGdbPath(self):
    """Returns the path to the android gdb."""
    return os.path.join(self.paths.src_root, "third_party", "android_tools",
                        "ndk", "toolchains", "arm-linux-androideabi-4.9",
                        "prebuilt", "linux-x86_64", "bin",
                        "arm-linux-androideabi-gdb")

  def _WaitForProcessIdAndStartGdb(self, process):
    """
    Waits until we see the process id from the remote device, starts up
    gdbserver on the remote device, and gdb on the local device.
    """
    # Wait until we see "PID"
    pid = self._GetProcessId(process)
    assert pid != 0
    # No longer need the logcat process.
    process.kill()
    # Disable python's processing of SIGINT while running gdb. Otherwise
    # control-c doesn't work well in gdb.
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    gdbserver_process = subprocess.Popen(self._CreateADBCommand(['shell',
                                                                 'gdbserver',
                                                                 '--attach',
                                                                 ':5039',
                                                                 pid]))
    atexit.register(_ExitIfNeeded, gdbserver_process)

    temp_dir = tempfile.mkdtemp()
    atexit.register(shutil.rmtree, temp_dir, True)

    gdbinit_path = os.path.join(temp_dir, 'gdbinit')
    _CreateGdbInit(temp_dir, gdbinit_path, self.paths.build_dir)

    _CreateSOLinks(temp_dir, self.paths.build_dir)

    # Wait a second for gdb to start up on the device. Without this the local
    # gdb starts before the remote side has registered the port.
    # TODO(sky): maybe we should try a couple of times and then give up?
    time.sleep(1)

    local_gdb_process = subprocess.Popen([self._GetLocalGdbPath(),
                                          "-x",
                                          gdbinit_path],
                                         cwd=temp_dir)
    atexit.register(_ExitIfNeeded, local_gdb_process)
    local_gdb_process.wait()
    signal.signal(signal.SIGINT, signal.SIG_DFL)

  def StartShell(self,
                 arguments,
                 stdout=None,
                 on_application_stop=None,
                 gdb=False):
    """
    Starts the mojo shell, passing it the given arguments.

    The |arguments| list must contain the "--origin=" arg from PrepareShellRun.
    If |stdout| is not None, it should be a valid argument for subprocess.Popen.
    """

    STDOUT_PIPE = "/data/data/%s/stdout.fifo" % self.target_package

    cmd = self._CreateADBCommand([
           'shell',
           'am',
           'start',
           '-S',
           '-a', 'android.intent.action.VIEW',
           '-n', '%s/%s.MojoShellActivity' % (self.target_package,
                                              'org.chromium.mojo.shell')])

    logcat_process = None

    if gdb:
      arguments += ['--wait-for-debugger']
      logcat_process = self.ShowLogs(stdout=subprocess.PIPE)

    parameters = []
    if stdout or on_application_stop:
      subprocess.check_call(self._CreateADBCommand(
          ['shell', 'rm', '-f', STDOUT_PIPE]))
      parameters.append('--fifo-path=%s' % STDOUT_PIPE)
      max_attempts = 5
      if '--wait-for-debugger' in arguments:
        max_attempts = 200
      self._ReadFifo(STDOUT_PIPE, stdout, on_application_stop,
                     max_attempts=max_attempts)

    # Extract map-origin arguments.
    map_parameters, other_parameters = _Split(arguments, _IsMapOrigin)
    parameters += other_parameters
    parameters += self._StartHttpServerForOriginMappings(map_parameters)

    if parameters:
      encodedParameters = json.dumps(parameters)
      cmd += ['--es', 'encodedParameters', encodedParameters]

    with open(os.devnull, 'w') as devnull:
      cmd_process = subprocess.Popen(cmd, stdout=devnull)
      if logcat_process:
        self._WaitForProcessIdAndStartGdb(logcat_process)
      cmd_process.wait()

  def StopShell(self):
    """Stops the mojo shell."""
    subprocess.check_call(self._CreateADBCommand(['shell',
                                                  'am',
                                                  'force-stop',
                                                  self.target_package]))

  def CleanLogs(self):
    """Cleans the logs on the device."""
    subprocess.check_call(self._CreateADBCommand(['logcat', '-c']))

  def ShowLogs(self, stdout=sys.stdout):
    """Displays the mojo shell logs and returns the process reading the logs."""
    logcat = subprocess.Popen(self._CreateADBCommand([
                               'logcat',
                               '-s',
                               ' '.join(LOGCAT_TAGS)]),
                              stdout=stdout)
    atexit.register(_ExitIfNeeded, logcat)
    return logcat


def _CreateGdbInit(tmp_dir, gdb_init_path, build_dir):
  """
  Creates the gdbinit file.

  Args:
    tmp_dir: the directory where the gdbinit and other files lives.
    gdb_init_path: path to gdbinit
    build_dir: path where build files are located.
  """
  gdbinit = ('target remote localhost:5039\n'
             'def reload-symbols\n'
             '  set solib-search-path %s:%s\n'
             'end\n'
             'def info-symbols\n'
             '  info sharedlibrary\n'
             'end\n'
             'reload-symbols\n'
             'echo \\n\\n'
             'You are now in gdb and need to type continue (or c) to continue '
             'execution.\\n'
             'gdb is in the directory %s\\n'
             'The following functions have been defined:\\n'
             'reload-symbols: forces reloading symbols. If after a crash you\\n'
             'still do not see symbols you likely need to create a link in\\n'
             'the directory you are in.\\n'
             'info-symbols: shows status of current shared libraries.\\n'
             'NOTE: you may need to type reload-symbols again after a '
             'crash.\\n\\n' % (tmp_dir, build_dir, tmp_dir))
  with open(gdb_init_path, 'w') as f:
    f.write(gdbinit)


def _CreateSOLinks(dest_dir, build_dir):
  """Creates links from files (eg. *.mojo) to the real .so for gdb to find."""
  # The files to create links for. The key is the name as seen on the device,
  # and the target an array of path elements as to where the .so lives (relative
  # to the output directory).
  # TODO(sky): come up with some way to automate this.
  files_to_link = {
    'html_viewer.mojo': ['libhtml_viewer', 'html_viewer_library.so'],
    'libmandoline_runner.so': ['mandoline_runner'],
  }
  for android_name, so_path in files_to_link.iteritems():
    src = os.path.join(build_dir, *so_path)
    if not os.path.isfile(src):
      print 'Expected file not found', src
      sys.exit(-1)
    os.symlink(src, os.path.join(dest_dir, android_name))
