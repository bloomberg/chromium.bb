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
import subprocess
import sys
import threading
import time
import urlparse

import SimpleHTTPServer
import SocketServer


# Tags used by the mojo shell application logs.
LOGCAT_TAGS = [
    'AndroidHandler',
    'MojoFileHelper',
    'MojoMain',
    'MojoShellActivity',
    'MojoShellApplication',
    'chromium',
]

MOJO_SHELL_PACKAGE_NAME = 'org.chromium.mojo.shell'

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

_logger = logging.getLogger()


class _SilentTCPServer(SocketServer.TCPServer):
  """
  A TCPServer that won't display any error, unless debugging is enabled. This is
  useful because the client might stop while it is fetching an URL, which causes
  spurious error messages.
  """
  def handle_error(self, request, client_address):
    """
    Override the base class method to have conditional logging.
    """
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
      """
      Override the base class method to disable logging.
      """
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
  """
  Exits |process| if it is still alive.
  """
  if process.poll() is None:
    process.kill()


class AndroidShell(object):
  """ Allows to set up and run a given mojo shell binary on an Android device.

  Args:
    shell_apk_path: path to the shell Android binary
    local_dir: directory where locally build Mojo apps will be served, optional
    adb_path: path to adb, optional if adb is in PATH
    target_device: device to run on, if multiple devices are connected
  """
  def __init__(
      self, shell_apk_path, local_dir=None, adb_path="adb", target_device=None,
      target_package=MOJO_SHELL_PACKAGE_NAME):
    self.shell_apk_path = shell_apk_path
    self.adb_path = adb_path
    self.local_dir = local_dir
    self.target_device = target_device
    self.target_package = target_package

  def _CreateADBCommand(self, args):
    adb_command = [self.adb_path]
    if self.target_device:
      adb_command.extend(['-s', self.target_device])
    adb_command.extend(args)
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
    """Starts an http server serving files from |path|. Returns the local
    url."""
    assert path
    httpd = _SilentTCPServer(('127.0.0.1', 0), _GetHandlerClassForPath(path))
    atexit.register(httpd.shutdown)

    http_thread = threading.Thread(target=httpd.serve_forever)
    http_thread.daemon = True
    http_thread.start()

    print 'Hosting %s at http://127.0.0.1:%d' % (path, httpd.server_address[1])
    return 'http://127.0.0.1:%d/' % self._MapPort(port, httpd.server_address[1])

  def _StartHttpServerForOriginMapping(self, mapping, port):
    """If |mapping| points at a local file starts an http server to serve files
    from the directory and returns the new mapping.

    This is intended to be called for every --map-origin value."""
    parts = mapping.split('=')
    if len(parts) != 2:
      return mapping
    dest = parts[1]
    # If the destination is a url, don't map it.
    if urlparse.urlparse(dest)[0]:
      return mapping
    # Assume the destination is a local file. Start a local server that
    # redirects to it.
    localUrl = self._StartHttpServerForDirectory(dest, port)
    print 'started server at %s for %s' % (dest, localUrl)
    return parts[0] + '=' + localUrl

  def _StartHttpServerForOriginMappings(self, map_parameters):
    """Calls _StartHttpServerForOriginMapping for every --map-origin
    argument."""
    if not map_parameters:
      return []

    original_values = list(itertools.chain(
        *map(lambda x: x[len(MAPPING_PREFIX):].split(','), map_parameters)))
    sorted(original_values)
    result = []
    for value in original_values:
      result.append(self._StartHttpServerForOriginMapping(value, 0))
    return [MAPPING_PREFIX + ','.join(result)]

  def PrepareShellRun(self, origin=None):
    """ Prepares for StartShell: runs adb as root and installs the apk.  If the
    origin specified is 'localhost', a local http server will be set up to serve
    files from the build directory along with port forwarding.

    Returns arguments that should be appended to shell argument list."""
    if 'cannot run as root' in subprocess.check_output(
        self._CreateADBCommand(['root'])):
      raise Exception("Unable to run adb as root.")
    subprocess.check_call(
        self._CreateADBCommand(['install', '-r', self.shell_apk_path, '-i',
                                self.target_package]))
    atexit.register(self.StopShell)

    extra_shell_args = []
    if origin is 'localhost':
      origin = self._StartHttpServerForDirectory(self.local_dir, 0)
    if origin:
      extra_shell_args.append("--origin=" + origin)
    return extra_shell_args

  def StartShell(self,
                 arguments,
                 stdout=None,
                 on_application_stop=None):
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
                                              MOJO_SHELL_PACKAGE_NAME)])

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
      subprocess.Popen(cmd, stdout=devnull).wait()

  def StopShell(self):
    """
    Stops the mojo shell.
    """
    subprocess.check_call(self._CreateADBCommand(['shell',
                                                  'am',
                                                  'force-stop',
                                                  self.target_package]))

  def CleanLogs(self):
    """
    Cleans the logs on the device.
    """
    subprocess.check_call(self._CreateADBCommand(['logcat', '-c']))

  def ShowLogs(self):
    """
    Displays the log for the mojo shell.

    Returns the process responsible for reading the logs.
    """
    logcat = subprocess.Popen(self._CreateADBCommand([
                               'logcat',
                               '-s',
                               ' '.join(LOGCAT_TAGS)]),
                              stdout=sys.stdout)
    atexit.register(_ExitIfNeeded, logcat)
    return logcat
