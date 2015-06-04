# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import itertools
import json
import logging
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import urlparse

from .paths import Paths

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                             os.pardir, 'build', 'android'))
from pylib import constants
from pylib.base import base_test_runner
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


def _ExitIfNeeded(process):
  """Exits |process| if it is still alive."""
  if process.poll() is None:
    process.kill()


class AndroidShell(object):
  """
  Used to set up and run a given mojo shell binary on an Android device.
  |config| is the mopy.config.Config for the build.
  """
  def __init__(self, config):
    self.adb_path = constants.GetAdbPath()
    self.paths = Paths(config)
    self.device = None
    self.target_package = apk_helper.GetPackageName(self.paths.apk_path)
    # This is used by decive_utils.Install to check if the apk needs updating.
    constants.SetOutputDirectory(self.paths.build_dir)

  # TODO(msw): Use pylib's adb_wrapper and device_utils instead.
  def _CreateADBCommand(self, args):
    adb_command = [self.adb_path, '-s', self.device.adb.GetDeviceSerial()]
    adb_command.extend(args)
    logging.getLogger().debug("Command: %s", " ".join(adb_command))
    return adb_command

  def _ReadFifo(self, path, pipe, on_fifo_closed, max_attempts=5):
    """
    Reads the fifo at |path| on the device and write the contents to |pipe|.
    Calls |on_fifo_closed| when the fifo is closed. This method will try to find
    the path up to |max_attempts|, waiting 1 second between each attempt. If it
    cannot find |path|, a exception will be raised.
    """
    def Run():
      def _WaitForFifo():
        for _ in xrange(max_attempts):
          if self.device.FileExists(path):
            return
          time.sleep(1)
        on_fifo_closed()
        raise Exception("Unable to find fifo.")
      _WaitForFifo()
      stdout_cat = subprocess.Popen(self._CreateADBCommand([
                                      'shell',
                                      'cat',
                                      path]),
                                    stdout=pipe)
      atexit.register(_ExitIfNeeded, stdout_cat)
      stdout_cat.wait()
      on_fifo_closed()

    thread = threading.Thread(target=Run, name="StdoutRedirector")
    thread.start()

  def _StartHttpServerForDirectory(self, path):
    test_server_helper = base_test_runner.BaseTestRunner(self.device, None)
    ports = test_server_helper.LaunchTestHttpServer(path)
    atexit.register(test_server_helper.ShutdownHelperToolsForTestSuite)
    print 'Hosting %s at http://127.0.0.1:%d' % (path, ports[1])
    return 'http://127.0.0.1:%d/' % ports[0]

  def _StartHttpServerForOriginMapping(self, mapping):
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
    localUrl = self._StartHttpServerForDirectory(dest)
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
      result.append(self._StartHttpServerForOriginMapping(value))
    return [MAPPING_PREFIX + ','.join(result)]

  def PrepareShellRun(self, origin=None, device=None, gdb=False):
    """
    Prepares for StartShell: runs adb as root and installs the apk as needed.
    If the origin specified is 'localhost', a local http server will be set up
    to serve files from the build directory along with port forwarding.
    |device| is the device to run on, if multiple devices are connected.
    Returns arguments that should be appended to shell argument list.
    """
    devices = device_utils.DeviceUtils.HealthyDevices()
    if device:
      self.device = next((d for d in devices if d == device), None)
      if not self.device:
        raise device_errors.DeviceUnreachableError(device)
    elif devices:
      self.device = devices[0]
    else:
      raise device_errors.NoDevicesError()

    logging.getLogger().debug("Using device: %s", self.device)
    self.device.EnableRoot()

    # TODO(msw): Install fails often, retry as needed; http://crbug.com/493900
    try:
      self.device.Install(self.paths.apk_path)
    except device_errors.CommandFailedError as e:
      logging.getLogger().error("APK install failed:\n%s", str(e))
      self.device.Install(self.paths.apk_path)

    extra_args = []
    if origin is 'localhost':
      origin = self._StartHttpServerForDirectory(self.paths.build_dir)
    if origin:
      extra_args.append("--origin=" + origin)

    if gdb:
      # Remote debugging needs a port forwarded.
      self.device.adb.Forward('tcp:5039', 'tcp:5039')

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

  def StartShell(self, arguments, stdout, on_application_stop, gdb=False):
    """
    Starts the shell with the given arguments, directing output to |stdout|.
    The |arguments| list must contain the "--origin=" arg from PrepareShellRun.
    """
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

    fifo_path = "/data/data/%s/stdout.fifo" % self.target_package
    subprocess.check_call(self._CreateADBCommand(
        ['shell', 'rm', '-f', fifo_path]))
    arguments.append('--fifo-path=%s' % fifo_path)
    max_attempts = 200 if '--wait-for-debugger' in arguments else 5
    self._ReadFifo(fifo_path, stdout, on_application_stop, max_attempts)

    # Extract map-origin arguments.
    parameters = [a for a in arguments if not a.startswith(MAPPING_PREFIX)]
    map_parameters = [a for a in arguments if a.startswith(MAPPING_PREFIX)]
    parameters += self._StartHttpServerForOriginMappings(map_parameters)

    if parameters:
      cmd += ['--es', 'encodedParameters', json.dumps(parameters)]

    atexit.register(self.StopShell)
    with open(os.devnull, 'w') as devnull:
      cmd_process = subprocess.Popen(cmd, stdout=devnull)
      if logcat_process:
        self._WaitForProcessIdAndStartGdb(logcat_process)
      cmd_process.wait()

  def StopShell(self):
    """Stops the mojo shell."""
    self.device.ForceStop(self.target_package)

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
