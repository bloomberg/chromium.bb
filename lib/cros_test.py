# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for running Chrome OS tests."""

from __future__ import print_function

import datetime
import os

from chromite.cli.cros import cros_chrome_sdk
from chromite.lib import chrome_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import device
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import vm


class CrOSTest(object):
  """Class for running Chrome OS tests."""

  def __init__(self, opts):
    """Initialize CrOSTest.

    Args:
      opts: command line options.
    """
    self.start_time = datetime.datetime.utcnow()

    self.start_vm = opts.start_vm
    self.cache_dir = opts.cache_dir

    self.build = opts.build
    self.deploy = opts.deploy
    self.nostrip = opts.nostrip
    self.build_dir = opts.build_dir
    self.mount = opts.mount

    self.catapult_tests = opts.catapult_tests
    self.guest = opts.guest

    self.autotest = opts.autotest
    self.tast = opts.tast
    self.results_dir = opts.results_dir
    self.test_that_args = opts.test_that_args
    self.test_timeout = opts.test_timeout

    self.remote_cmd = opts.remote_cmd
    self.host_cmd = opts.host_cmd
    self.cwd = opts.cwd
    self.files = opts.files
    self.files_from = opts.files_from
    self.as_chronos = opts.as_chronos
    self.args = opts.args[1:] if opts.args else None

    self.results_src = opts.results_src
    self.results_dest_dir = opts.results_dest_dir

    self.chrome_test = opts.chrome_test
    if self.chrome_test:
      self.chrome_test_target = os.path.basename(opts.args[1])
      self.chrome_test_deploy_target_dir = '/usr/local/chrome_test'
    else:
      self.chrome_test_target = None
      self.chrome_test_deploy_target_dir = None
    self.staging_dir = None

    self._device = device.Device.Create(opts)

  def __del__(self):
    self._StopVM()

    logging.info('Time elapsed: %s',
                 datetime.datetime.utcnow() - self.start_time)

  def Run(self):
    """Start a VM, build/deploy, run tests, stop the VM."""
    if self._device.is_vm:
      self._StartVM()
    else:
      self._device.WaitForBoot()

    self._Build()
    self._Deploy()

    returncode = self._RunTests()

    self._StopVM()
    return returncode

  def _StartVM(self):
    """Start a VM if necessary.

    If --start-vm is specified, we launch a new VM, otherwise we use an
    existing VM.
    """
    if not self._device.is_vm:
      return

    if not self._device.IsRunning():
      self.start_vm = True

    if self.start_vm:
      self._device.Start()

  def _StopVM(self):
    """Stop the VM if necessary.

    If --start-vm was specified, we launched this VM, so we now stop it.
    """
    if self._device and self.start_vm:
      self._device.Stop()

  def _Build(self):
    """Build chrome."""
    if not self.build:
      return

    build_target = self.chrome_test_target or 'chromiumos_preflight'
    self._device.RunCommand(['autoninja', '-C', self.build_dir,
                             build_target])

  def _Deploy(self):
    """Deploy binary files to device."""
    if not self.build and not self.deploy:
      return

    if self.chrome_test:
      self._DeployChromeTest()
    else:
      self._DeployChrome()

  def _DeployChrome(self):
    """Deploy chrome."""
    deploy_cmd = [
        'deploy_chrome', '--force',
        '--build-dir', self.build_dir,
        '--process-timeout', '180',
        '--to', self._device.device
    ]
    if self._device.ssh_port:
      deploy_cmd += ['--port', str(self._device.ssh_port)]
    if self._device.board:
      deploy_cmd += ['--board', self._device.board]
    if self.cache_dir:
      deploy_cmd += ['--cache-dir', self.cache_dir]
    if self.nostrip:
      deploy_cmd += ['--nostrip']
    if self.mount:
      deploy_cmd += ['--mount']
    self._device.RunCommand(deploy_cmd)
    self._device.WaitForBoot()

  def _DeployChromeTest(self):
    """Deploy chrome test binary and its runtime files to device."""
    src_dir = os.path.dirname(os.path.dirname(self.build_dir))
    self._DeployCopyPaths(src_dir, self.chrome_test_deploy_target_dir,
                          chrome_util.GetChromeTestCopyPaths(
                              self.build_dir, self.chrome_test_target))

  def _DeployCopyPaths(self, host_src_dir, remote_target_dir, copy_paths):
    """Deploy files in copy_paths to device.

    Args:
      host_src_dir: Source dir on the host that files in |copy_paths| are
                    relative to.
      remote_target_dir: Target dir on the remote device that the files in
                         |copy_paths| are copied to.
      copy_paths: A list of chrome_utils.Path of files to be copied.
    """
    with osutils.TempDir(set_global=True) as tempdir:
      self.staging_dir = tempdir
      strip_bin = None
      chrome_util.StageChromeFromBuildDir(
          self.staging_dir, host_src_dir, strip_bin, copy_paths=copy_paths)

      if self._device.remote.HasRsync():
        self._device.remote.CopyToDevice(
            '%s/' % os.path.abspath(self.staging_dir), remote_target_dir,
            mode='rsync', inplace=True, compress=True, debug_level=logging.INFO)
      else:
        self._device.remote.CopyToDevice(
            '%s/' % os.path.abspath(self.staging_dir), remote_target_dir,
            mode='scp', debug_level=logging.INFO)

  def _RunCatapultTests(self):
    """Run catapult tests matching a pattern using run_tests.

    Returns:
      cros_build_lib.CommandResult object.
    """

    browser = 'system-guest' if self.guest else 'system'
    return self._device.RemoteCommand([
        'python',
        '/usr/local/telemetry/src/third_party/catapult/telemetry/bin/run_tests',
        '--browser=%s' % browser,
    ] + self.catapult_tests, stream_output=True)

  def _RunAutotest(self):
    """Run an autotest using test_that.

    Returns:
      cros_build_lib.CommandResult object.
    """
    cmd = ['test_that']
    if self._device.board:
      cmd += ['--board', self._device.board]
    if self.results_dir:
      cmd += ['--results_dir', path_util.ToChrootPath(self.results_dir)]
    if self._device.private_key:
      cmd += ['--ssh_private_key',
              path_util.ToChrootPath(self._device.private_key)]
    if self._device.log_level == 'debug':
      cmd += ['--debug']
    if self.test_that_args:
      cmd += self.test_that_args[1:]
    cmd += [
        '--no-quickmerge',
        '--ssh_options', '-F /dev/null -i /dev/null',
    ]
    if self._device.ssh_port:
      cmd += ['%s:%d' % (self._device.device, self._device.ssh_port)]
    else:
      cmd += [self._device.device]
    cmd += self.autotest
    return self._device.RunCommand(
        cmd, enter_chroot=not cros_build_lib.IsInsideChroot())

  def _RunTastTests(self):
    """Run Tast tests.

    Returns:
      cros_build_lib.CommandResult object.
    """
    # Try using the Tast binaries that the SimpleChrome SDK downloads
    # automatically.
    tast_cache_dir = cros_chrome_sdk.SDKFetcher.GetCachePath(
        'chromeos-base', self.cache_dir, self._device.board)
    if tast_cache_dir:
      tast_bin_dir = os.path.join(tast_cache_dir, 'tast-cmd', 'usr', 'bin')
      cmd = [os.path.join(tast_bin_dir, 'tast')]
      need_chroot = False
    else:
      # Silently fall back to using the chroot if there's no SimpleChrome SDK
      # present.
      cmd = ['tast']
      need_chroot = True

    if self._device.log_level == 'debug':
      cmd += ['-verbose']
    cmd += ['run', '-build=false', '-waituntilready',]
    if not need_chroot:
      # The test runner needs to be pointed to the location of the test files
      # when we're using those in the SimpleChrome cache.
      remote_runner_path = os.path.join(tast_bin_dir, 'remote_test_runner')
      remote_bundle_dir = os.path.join(
          tast_cache_dir, 'tast-remote-tests-cros', 'usr', 'libexec', 'tast',
          'bundles', 'remote')
      remote_data_dir = os.path.join(
          tast_cache_dir, 'tast-remote-tests-cros', 'usr', 'share', 'tast',
          'data')
      private_key = (self._device.private_key or
                     self._device.remote.GetAgent().private_key)
      assert private_key, 'ssh private key not found.'
      cmd += [
          '-remoterunner=%s' % remote_runner_path,
          '-remotebundledir=%s' % remote_bundle_dir,
          '-remotedatadir=%s' % remote_data_dir,
          # The dev server has trouble downloading assets from Google Storage
          # from outside the chroot.
          '-ephemeraldevserver=false',
          '-keyfile', private_key,
      ]
    if self.test_timeout > 0:
      cmd += ['-timeout=%d' % self.test_timeout]
    if self._device.is_vm:
      cmd += ['-extrauseflags=tast_vm']
    if self.results_dir:
      results_dir = self.results_dir
      if need_chroot:
        results_dir = path_util.ToChrootPath(self.results_dir)
      cmd += ['-resultsdir', results_dir]
    if self._device.ssh_port:
      cmd += ['%s:%d' % (self._device.device, self._device.ssh_port)]
    else:
      cmd += [self._device.device]
    cmd += self.tast
    return self._device.RunCommand(
        cmd, enter_chroot=need_chroot and not cros_build_lib.IsInsideChroot())

  def _RunTests(self):
    """Run tests.

    Run user-specified tests, catapult tests, tast tests, autotest, or the
    default, vm_sanity.

    Returns:
      Command execution return code.
    """
    if self.remote_cmd:
      result = self._RunDeviceCmd()
    elif self.host_cmd:
      # Don't raise an exception if the command fails.
      result = self._device.RunCommand(self.args, error_code_ok=True)
    elif self.catapult_tests:
      result = self._RunCatapultTests()
    elif self.autotest:
      result = self._RunAutotest()
    elif self.tast:
      result = self._RunTastTests()
    elif self.chrome_test:
      result = self._RunChromeTest()
    else:
      result = self._device.RemoteCommand(
          ['/usr/local/autotest/bin/vm_sanity.py'], stream_output=True)

    self._FetchResults()

    name = self.args[0] if self.args else 'Test process'
    logging.info('%s exited with status code %d.', name, result.returncode)

    return result.returncode

  def _FetchResults(self):
    """Fetch results files/directories."""
    if not self.results_src:
      return
    osutils.SafeMakedirs(self.results_dest_dir)
    for src in self.results_src:
      logging.info('Fetching %s to %s', src, self.results_dest_dir)
      self._device.remote.CopyFromDevice(src=src, dest=self.results_dest_dir,
                                         mode='scp', error_code_ok=True,
                                         debug_level=logging.INFO)

  def _RunDeviceCmd(self):
    """Run a command on the device.

    Copy src files to /usr/local/cros_test/, change working directory to
    self.cwd, run the command in self.args, and cleanup.

    Returns:
      cros_build_lib.CommandResult object.
    """
    DEST_BASE = '/usr/local/cros_test'
    files = FileList(self.files, self.files_from)
    # Copy files, preserving the directory structure.
    copy_paths = []
    for f in files:
      is_exe = os.path.isfile(f) and os.access(f, os.X_OK)
      has_exe = False
      if os.path.isdir(f):
        if not f.endswith('/'):
          f += '/'
        for sub_dir, _, sub_files in os.walk(f):
          for sub_file in sub_files:
            if os.access(os.path.join(sub_dir, sub_file), os.X_OK):
              has_exe = True
              break
          if has_exe:
            break
      copy_paths.append(chrome_util.Path(f, exe=is_exe or has_exe))
    if copy_paths:
      self._DeployCopyPaths(os.getcwd(), DEST_BASE, copy_paths)

    # Make cwd an absolute path (if it isn't one) rooted in DEST_BASE.
    cwd = self.cwd
    if files and not (cwd and os.path.isabs(cwd)):
      cwd = os.path.join(DEST_BASE, cwd) if cwd else DEST_BASE
      self._device.RemoteCommand(['mkdir', '-p', cwd])

    if self.as_chronos:
      # This authorizes the test ssh keys with chronos.
      self._device.RemoteCommand(['cp', '-r', '/root/.ssh/',
                                  '/home/chronos/user/'])
      if files:
        # The trailing ':' after the user also changes the group to the user's
        # primary group.
        self._device.RemoteCommand(['chown', '-R', 'chronos:', DEST_BASE])

    user = 'chronos' if self.as_chronos else None
    if cwd:
      # Run the remote command with cwd.
      cmd = '"cd %s && %s"' % (cwd, ' '.join(self.args))
      # Pass shell=True because of && in the cmd.
      result = self._device.RemoteCommand(cmd, stream_output=True, shell=True,
                                          remote_user=user)
    else:
      result = self._device.RemoteCommand(self.args, stream_output=True,
                                          remote_user=user)

    # Cleanup.
    if files:
      self._device.RemoteCommand(['rm', '-rf', DEST_BASE])

    return result

  def _RunChromeTest(self):
    # Stop UI in case the test needs to grab GPU.
    self._device.RemoteCommand('stop ui')

    # Send a user activity ping to powerd to light up the display.
    self._device.RemoteCommand(['dbus-send', '--system', '--type=method_call',
                                '--dest=org.chromium.PowerManager',
                                '/org/chromium/PowerManager',
                                'org.chromium.PowerManager.HandleUserActivity',
                                'int32:0'])

    # Run test.
    chrome_src_dir = os.path.dirname(os.path.dirname(self.build_dir))
    test_binary = os.path.relpath(
        os.path.join(self.build_dir, self.chrome_test_target), chrome_src_dir)
    test_args = self.args[1:]
    command = 'cd %s && su chronos -c -- "%s %s"' % \
        (self.chrome_test_deploy_target_dir, test_binary, ' '.join(test_args))
    result = self._device.RemoteCommand(command, stream_output=True)
    return result


def ParseCommandLine(argv):
  """Parse the command line.

  Args:
    argv: Command arguments.

  Returns:
    List of parsed options for CrOSTest.
  """

  parser = vm.VM.GetParser()
  parser.add_argument('--start-vm', action='store_true', default=False,
                      help='Start a new VM before running tests.')
  parser.add_argument('--catapult-tests', nargs='+',
                      help='Catapult test pattern to run, passed to run_tests.')
  parser.add_argument('--autotest', nargs='+',
                      help='Autotest test pattern to run, passed to test_that.')
  parser.add_argument('--tast', nargs='+',
                      help='Tast test pattern to run, passed to tast. '
                      'See go/tast-running for patterns.')
  parser.add_argument('--chrome-test', action='store_true', default=False,
                      help='Run chrome test on device. The first arg in the '
                      'remote command should be the test binary name, such as '
                      'interactive_ui_tests. It is used for building and '
                      'collecting runtime deps files.')
  parser.add_argument('--guest', action='store_true', default=False,
                      help='Run tests in incognito mode.')
  parser.add_argument('--build-dir', type='path',
                      help='Directory for building and deploying chrome.')
  parser.add_argument('--build', action='store_true', default=False,
                      help='Before running tests, build chrome using ninja, '
                      '--build-dir must be specified.')
  parser.add_argument('--deploy', action='store_true', default=False,
                      help='Before running tests, deploy chrome, '
                      '--build-dir must be specified.')
  parser.add_argument('--nostrip', action='store_true', default=False,
                      help="Don't strip symbols from binaries if deploying.")
  parser.add_argument('--mount', action='store_true', default=False,
                      help='Deploy chrome to the default target directory and '
                      'bind it to the default mount directory. Useful for '
                      'large Chrome binaries.')
  # type='path' converts a relative path for cwd into an absolute one on the
  # host, which we don't want.
  parser.add_argument('--cwd', help='Change working directory. '
                      'An absolute path or a path relative to CWD on the host.')
  parser.add_argument('--files', default=[], action='append',
                      help='Files to scp to the device.')
  parser.add_argument('--files-from', type='path',
                      help='File with list of files to copy.')
  parser.add_argument('--results-src', default=[], action='append',
                      help='Files/Directories to copy from '
                      'the device into CWD after running the test.')
  parser.add_argument('--results-dest-dir', type='path',
                      help='Destination directory to copy results to.')
  parser.add_argument('--remote-cmd', action='store_true', default=False,
                      help='Run a command on the device.')
  parser.add_argument('--as-chronos', action='store_true',
                      help='Runs the remote test as the chronos user on '
                           'the device. Only supported for --remote-cmd tests. '
                           'Runs as root if not set.')
  parser.add_argument('--host-cmd', action='store_true', default=False,
                      help='Run a command on the host.')
  parser.add_argument('--results-dir', type='path',
                      help='Autotest results directory.')
  parser.add_argument('--test_that-args', action='append_option_value',
                      help='Args to pass directly to test_that for autotest.')
  parser.add_argument('--test-timeout', type=int, default=0,
                      help='Timeout for running all tests (for --tast).')

  opts = parser.parse_args(argv)

  if opts.chrome_test:
    if not opts.args:
      parser.error('Must specify a test command with --chrome-test')

    if not opts.build_dir:
      opts.build_dir = os.path.dirname(opts.args[1])

  if opts.build or opts.deploy:
    if not opts.build_dir:
      parser.error('Must specify --build-dir with --build or --deploy.')
    if not os.path.isdir(opts.build_dir):
      parser.error('%s is not a directory.' % opts.build_dir)

  if opts.results_src:
    for src in opts.results_src:
      if not os.path.isabs(src):
        parser.error('results-src must be absolute.')
    if not opts.results_dest_dir:
      parser.error('results-dest-dir must be specified with results-src.')
  if opts.results_dest_dir:
    if not opts.results_src:
      parser.error('results-src must be specified with results-dest-dir.')
    if os.path.isfile(opts.results_dest_dir):
      parser.error('results-dest-dir %s is an existing file.'
                   % opts.results_dest_dir)

  # Ensure command is provided. For e.g. to copy out to the device and run
  # out/unittest:
  # cros_run_test --files out --cwd out --cmd -- ./unittest
  # Treat --cmd as --remote-cmd.
  opts.remote_cmd = opts.remote_cmd or opts.cmd
  if (opts.remote_cmd or opts.host_cmd) and len(opts.args) < 2:
    parser.error('Must specify test command to run.')
  if opts.as_chronos and not opts.remote_cmd:
    parser.error('as-chronos only supported when running test commands.')
  # Verify additional args.
  if opts.args:
    if not opts.remote_cmd and not opts.host_cmd and not opts.chrome_test:
      parser.error('Additional args may be specified with either '
                   '--remote-cmd or --host-cmd or --chrome-test: %s' %
                   opts.args)
    if opts.args[0] != '--':
      parser.error('Additional args must start with \'--\': %s' % opts.args)

  # Verify CWD.
  if opts.cwd:
    if opts.cwd.startswith('..'):
      parser.error('cwd cannot start with ..')
    if not os.path.isabs(opts.cwd) and not opts.files and not opts.files_from:
      parser.error('cwd must be an absolute path if '
                   '--files or --files-from is not specified')

  # Verify files.
  if opts.files_from:
    if opts.files:
      parser.error('--files and --files-from cannot both be specified')
    if not os.path.isfile(opts.files_from):
      parser.error('%s is not a file' % opts.files_from)
  files = FileList(opts.files, opts.files_from)
  for f in files:
    if os.path.isabs(f):
      parser.error('%s should be a relative path' % f)
    # Restrict paths to under CWD on the host. See crbug.com/829612.
    if f.startswith('..'):
      parser.error('%s cannot start with ..' % f)
    if not os.path.exists(f):
      parser.error('%s does not exist' % f)

  return opts


def FileList(files, files_from):
  """Get list of files from command line args --files and --files-from.

  Args:
    files: files specified directly on the command line.
    files_from: files specified in a file.

  Returns:
    Contents of files_from if it exists, otherwise files.
  """
  if files_from and os.path.isfile(files_from):
    with open(files_from) as f:
      files = [line.rstrip() for line in f]
  return files
