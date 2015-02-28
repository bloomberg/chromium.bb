# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging
import os

from pylib import constants
from pylib import ports
from pylib.base import test_run
from pylib.device import device_errors
from pylib.gtest import gtest_test_instance

from pylib.local import local_test_server_spawner
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_test_run
from pylib.utils import apk_helper
from pylib.utils import device_temp_file

_COMMAND_LINE_FLAGS_SUPPORTED = True

_EXTRA_COMMAND_LINE_FILE = (
    'org.chromium.native_test.ChromeNativeTestActivity.CommandLineFile')
_EXTRA_COMMAND_LINE_FLAGS = (
    'org.chromium.native_test.ChromeNativeTestActivity.CommandLineFlags')

_MAX_SHARD_SIZE = 256

# TODO(jbudorick): Move this up to the test instance if the net test server is
# handled outside of the APK for the remote_device environment.
_SUITE_REQUIRES_TEST_SERVER_SPAWNER = [
  'content_unittests', 'content_browsertests', 'net_unittests', 'unit_tests'
]

class _ApkDelegate(object):
  def __init__(self, apk):
    self._apk = apk
    self._package = apk_helper.GetPackageName(self._apk)
    self._runner = apk_helper.GetInstrumentationName(self._apk)
    self._component = '%s/%s' % (self._package, self._runner)
    self._enable_test_server_spawner = False

  def Install(self, device):
    device.Install(self._apk)

  def RunWithFlags(self, device, flags, **kwargs):
    with device_temp_file.DeviceTempFile(device.adb) as command_line_file:
      device.WriteFile(command_line_file.name, '_ %s' % flags)

      extras = {
        _EXTRA_COMMAND_LINE_FILE: command_line_file.name,
      }

      return device.StartInstrumentation(
          self._component, extras=extras, raw=False, **kwargs)

  def Clear(self, device):
    device.ClearApplicationState(self._package)


class _ExeDelegate(object):
  def __init__(self, exe, tr):
    self._exe_host_path = exe
    self._exe_file_name = os.path.split(exe)[-1]
    self._exe_device_path = '%s/%s' % (
        constants.TEST_EXECUTABLE_DIR, self._exe_file_name)
    deps_host_path = self._exe_host_path + '_deps'
    if os.path.exists(deps_host_path):
      self._deps_host_path = deps_host_path
      self._deps_device_path = self._exe_device_path + '_deps'
    else:
      self._deps_host_path = None
    self._test_run = tr

  def Install(self, device):
    # TODO(jbudorick): Look into merging this with normal data deps pushing if
    # executables become supported on nonlocal environments.
    host_device_tuples = [(self._exe_host_path, self._exe_device_path)]
    if self._deps_host_path:
      host_device_tuples.append((self._deps_host_path, self._deps_device_path))
    device.PushChangedFiles(host_device_tuples)

  def RunWithFlags(self, device, flags, **kwargs):
    cmd = [
        self._test_run.GetTool(device).GetTestWrapper(),
        self._exe_device_path,
        flags,
    ]
    cwd = constants.TEST_EXECUTABLE_DIR

    env = {
      'LD_LIBRARY_PATH':
          '%s/%s_deps' % (constants.TEST_EXECUTABLE_DIR, self._exe_file_name),
    }
    try:
      gcov_strip_depth = os.environ['NATIVE_COVERAGE_DEPTH_STRIP']
      external = device.GetExternalStoragePath()
      env['GCOV_PREFIX'] = '%s/gcov' % external
      env['GCOV_PREFIX_STRIP'] = gcov_strip_depth
    except (device_errors.CommandFailedError, KeyError):
      pass

    # TODO(jbudorick): Switch to just RunShellCommand once perezju@'s CL
    # for long shell commands lands.
    with device_temp_file.DeviceTempFile(device.adb) as script_file:
      script_contents = ' '.join(cmd)
      logging.info('script contents: %r' % script_contents)
      device.WriteFile(script_file.name, script_contents)
      output = device.RunShellCommand(['sh', script_file.name], cwd=cwd,
                                      env=env, **kwargs)
    return output

  def Clear(self, device):
    try:
      device.KillAll(self._exe_file_name, blocking=True, timeout=30, retries=0)
    except device_errors.CommandFailedError:
      # Raised if there is no process with the given name, which in this case
      # is all we care about.
      pass


class LocalDeviceGtestRun(local_device_test_run.LocalDeviceTestRun):

  def __init__(self, env, test_instance):
    assert isinstance(env, local_device_environment.LocalDeviceEnvironment)
    assert isinstance(test_instance, gtest_test_instance.GtestTestInstance)
    super(LocalDeviceGtestRun, self).__init__(env, test_instance)

    if self._test_instance.apk:
      self._delegate = _ApkDelegate(self._test_instance.apk)
    elif self._test_instance.exe:
      self._delegate = _ExeDelegate(self, self._test_instance.exe)

    self._servers = {}

  #override
  def TestPackage(self):
    return self._test_instance._suite

  #override
  def SetUp(self):

    def individual_device_set_up(dev, host_device_tuples):
      # Install test APK.
      self._delegate.Install(dev)

      # Push data dependencies.
      external_storage = dev.GetExternalStoragePath()
      host_device_tuples = [
          (h, d if d is not None else external_storage)
          for h, d in host_device_tuples]
      dev.PushChangedFiles(host_device_tuples)

      self._servers[str(dev)] = []
      if self.TestPackage() in _SUITE_REQUIRES_TEST_SERVER_SPAWNER:
        self._servers[str(dev)].append(
            local_test_server_spawner.LocalTestServerSpawner(
                ports.AllocateTestServerPort(), dev, self.GetTool(dev)))

      for s in self._servers[str(dev)]:
        s.SetUp()

    self._env.parallel_devices.pMap(individual_device_set_up,
                                    self._test_instance.GetDataDependencies())

  #override
  def _ShouldShard(self):
    return True

  #override
  def _CreateShards(self, tests):
    device_count = len(self._env.devices)
    shards = []
    for i in xrange(0, device_count):
      unbounded_shard = tests[i::device_count]
      shards += [unbounded_shard[j:j+_MAX_SHARD_SIZE]
                 for j in xrange(0, len(unbounded_shard), _MAX_SHARD_SIZE)]
    return [':'.join(s) for s in shards]

  #override
  def _GetTests(self):
    tests = self._delegate.RunWithFlags(
        self._env.devices[0], '--gtest_list_tests')
    tests = gtest_test_instance.ParseGTestListTests(tests)
    tests = self._test_instance.FilterTests(tests)
    return tests

  #override
  def _RunTest(self, device, test):
    # Run the test.
    output = self._delegate.RunWithFlags(device, '--gtest_filter=%s' % test,
                                         timeout=900, retries=0)
    for s in self._servers[str(device)]:
      s.Reset()
    self._delegate.Clear(device)

    # Parse the output.
    # TODO(jbudorick): Transition test scripts away from parsing stdout.
    results = self._test_instance.ParseGTestOutput(output)
    return results

  #override
  def TearDown(self):
    def individual_device_tear_down(dev):
      for s in self._servers[str(dev)]:
        s.TearDown()

    self._env.parallel_devices.pMap(individual_device_tear_down)

