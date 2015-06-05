# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the commandline module."""

from __future__ import print_function

import argparse
import cPickle
import signal
import os
import sys

from chromite.cbuildbot import constants
from chromite.cli import command
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import path_util
from chromite.lib import workspace_lib

# pylint: disable=protected-access


class TestShutDownException(cros_test_lib.TestCase):
  """Test that ShutDownException can be pickled."""

  def testShutDownException(self):
    """Test that ShutDownException can be pickled."""
    ex = commandline._ShutDownException(signal.SIGTERM, 'Received SIGTERM')
    ex2 = cPickle.loads(cPickle.dumps(ex))
    self.assertEqual(ex.signal, ex2.signal)
    self.assertEqual(ex.message, ex2.message)


class GSPathTest(cros_test_lib.OutputTestCase):
  """Test type=gs_path normalization functionality."""

  GS_REL_PATH = 'bucket/path/to/artifacts'

  @staticmethod
  def _ParseCommandLine(argv):
    parser = commandline.ArgumentParser()
    parser.add_argument('-g', '--gs-path', type='gs_path',
                        help='GS path that contains the chrome to deploy.')
    return parser.parse_args(argv)

  def _RunGSPathTestCase(self, raw, parsed):
    options = self._ParseCommandLine(['--gs-path', raw])
    self.assertEquals(options.gs_path, parsed)

  def testNoGSPathCorrectionNeeded(self):
    """Test case where GS path correction is not needed."""
    gs_path = '%s/%s' % (gs.BASE_GS_URL, self.GS_REL_PATH)
    self._RunGSPathTestCase(gs_path, gs_path)

  def testTrailingSlashRemoval(self):
    """Test case where GS path ends with /."""
    gs_path = '%s/%s/' % (gs.BASE_GS_URL, self.GS_REL_PATH)
    self._RunGSPathTestCase(gs_path, gs_path.rstrip('/'))

  def testDuplicateSlashesRemoved(self):
    """Test case where GS path contains many / in a row."""
    self._RunGSPathTestCase(
        '%s/a/dir/with//////////slashes' % gs.BASE_GS_URL,
        '%s/a/dir/with/slashes' % gs.BASE_GS_URL)

  def testRelativePathsRemoved(self):
    """Test case where GS path contain /../ logic."""
    self._RunGSPathTestCase(
        '%s/a/dir/up/here/.././../now/down/there' % gs.BASE_GS_URL,
        '%s/a/dir/now/down/there' % gs.BASE_GS_URL)

  def testCorrectionNeeded(self):
    """Test case where GS path correction is needed."""
    self._RunGSPathTestCase(
        '%s/%s/' % (gs.PRIVATE_BASE_HTTPS_URL, self.GS_REL_PATH),
        '%s/%s' % (gs.BASE_GS_URL, self.GS_REL_PATH))

  def testInvalidPath(self):
    """Path cannot be normalized."""
    with self.OutputCapturer():
      self.assertRaises2(
          SystemExit, self._RunGSPathTestCase, 'http://badhost.com/path', '',
          check_attrs={'code': 2})


class BoolTest(cros_test_lib.TestCase):
  """Test type='bool' functionality."""

  @staticmethod
  def _ParseCommandLine(argv):
    parser = commandline.ArgumentParser()
    parser.add_argument('-e', '--enable', type='bool',
                        help='Boolean Argument.')
    return parser.parse_args(argv)

  def _RunBoolTestCase(self, enable, expected):
    options = self._ParseCommandLine(['--enable', enable])
    self.assertEquals(options.enable, expected)

  def testBoolTrue(self):
    """Test case setting the value to true."""
    self._RunBoolTestCase('True', True)
    self._RunBoolTestCase('1', True)
    self._RunBoolTestCase('true', True)
    self._RunBoolTestCase('yes', True)
    self._RunBoolTestCase('TrUe', True)

  def testBoolFalse(self):
    """Test case setting the value to false."""
    self._RunBoolTestCase('False', False)
    self._RunBoolTestCase('0', False)
    self._RunBoolTestCase('false', False)
    self._RunBoolTestCase('no', False)
    self._RunBoolTestCase('FaLse', False)


class DeviceParseTest(cros_test_lib.OutputTestCase):
  """Test device parsing functionality."""

  _ALL_SCHEMES = (commandline.DEVICE_SCHEME_FILE,
                  commandline.DEVICE_SCHEME_SSH,
                  commandline.DEVICE_SCHEME_USB)

  def _CheckDeviceParse(self, device_input, scheme, username=None,
                        hostname=None, port=None, path=None):
    """Checks that parsing a device input gives the expected result.

    Args:
      device_input: String input specifying a device.
      scheme: String expected scheme.
      username: String expected username or None.
      hostname: String expected hostname or None.
      port: Int expected port or None.
      path: String expected path or None.
    """
    parser = commandline.ArgumentParser()
    parser.add_argument('device', type=commandline.DeviceParser(scheme))
    device = parser.parse_args([device_input]).device
    self.assertEqual(device.scheme, scheme)
    self.assertEqual(device.username, username)
    self.assertEqual(device.hostname, hostname)
    self.assertEqual(device.port, port)
    self.assertEqual(device.path, path)

  def _CheckDeviceParseFails(self, device_input, schemes=_ALL_SCHEMES):
    """Checks that parsing a device input fails.

    Args:
      device_input: String input specifying a device.
      schemes: A scheme or list of allowed schemes, by default allows all.
    """
    parser = commandline.ArgumentParser()
    parser.add_argument('device', type=commandline.DeviceParser(schemes))
    with self.OutputCapturer():
      self.assertRaises2(SystemExit, parser.parse_args, [device_input])

  def testNoDevice(self):
    """Verify that an empty device specification fails."""
    self._CheckDeviceParseFails('')

  def testSshScheme(self):
    """Verify that SSH scheme-only device specification fails."""
    self._CheckDeviceParseFails('ssh://')

  def testSshHostname(self):
    """Test SSH hostname-only device specification."""
    self._CheckDeviceParse('192.168.1.200',
                           scheme=commandline.DEVICE_SCHEME_SSH,
                           hostname='192.168.1.200')

  def testSshUsernameHostname(self):
    """Test SSH username and hostname device specification."""
    self._CheckDeviceParse('me@foo_host',
                           scheme=commandline.DEVICE_SCHEME_SSH,
                           username='me',
                           hostname='foo_host')

  def testSshUsernameHostnamePort(self):
    """Test SSH username, hostname, and port device specification."""
    self._CheckDeviceParse('me@foo_host:4500',
                           scheme=commandline.DEVICE_SCHEME_SSH,
                           username='me',
                           hostname='foo_host',
                           port=4500)

  def testSshSchemeUsernameHostnamePort(self):
    """Test SSH scheme, username, hostname, and port device specification."""
    self._CheckDeviceParse('ssh://me@foo_host:4500',
                           scheme=commandline.DEVICE_SCHEME_SSH,
                           username='me',
                           hostname='foo_host',
                           port=4500)

  def testUsbScheme(self):
    """Test USB scheme-only device specification."""
    self._CheckDeviceParse('usb://', scheme=commandline.DEVICE_SCHEME_USB)

  def testUsbSchemePath(self):
    """Test USB scheme and path device specification."""
    self._CheckDeviceParse('usb://path/to/my/device',
                           scheme=commandline.DEVICE_SCHEME_USB,
                           path='path/to/my/device')

  def testFileScheme(self):
    """Verify that file scheme-only device specification fails."""
    self._CheckDeviceParseFails('file://')

  def testFileSchemePath(self):
    """Test file scheme and path device specification."""
    self._CheckDeviceParse('file://foo/bar',
                           scheme=commandline.DEVICE_SCHEME_FILE,
                           path='foo/bar')

  def testAbsolutePath(self):
    """Verify that an absolute path defaults to file scheme."""
    self._CheckDeviceParse('/path/to/my/device',
                           scheme=commandline.DEVICE_SCHEME_FILE,
                           path='/path/to/my/device')

  def testUnsupportedScheme(self):
    """Verify that an unsupported scheme fails."""
    self._CheckDeviceParseFails('ssh://192.168.1.200',
                                schemes=commandline.DEVICE_SCHEME_USB)
    self._CheckDeviceParseFails('usb://path/to/my/device',
                                schemes=[commandline.DEVICE_SCHEME_SSH,
                                         commandline.DEVICE_SCHEME_FILE])

  def testUnknownScheme(self):
    """Verify that an unknown scheme fails."""
    self._CheckDeviceParseFails('ftp://192.168.1.200')

  def testSchemeCaseInsensitive(self):
    """Verify that schemes are case-insensitive."""
    self._CheckDeviceParse('SSH://foo_host',
                           scheme=commandline.DEVICE_SCHEME_SSH,
                           hostname='foo_host')


class NormalizeWorkspacePathTest(cros_test_lib.WorkspaceTestCase):
  """Tests for NormalizeWorkspacePath() and associated functions."""

  def setUp(self):
    self.CreateWorkspace()
    # By default set the CWD to be the workspace directory.
    self.cwd_mock = self.PatchObject(os, 'getcwd')
    self.cwd_mock.return_value = self.workspace_path

  def _VerifyNormalized(self, path, expected, **kwargs):
    """Verifies tests on NormalizeWorkspacePath().

    Args:
      path: Input path to test.
      expected: Expected output.
      kwargs: Keyword args for NormalizeWorkspacePath().
    """
    self.assertEqual(expected,
                     commandline.NormalizeWorkspacePath(path, **kwargs))


  def testLocatorConversion(self):
    """Tests NormalizeWorkspacePath() conversion to a locator."""
    # Relative paths.
    self._VerifyNormalized('a', '//a')
    self._VerifyNormalized('a/b', '//a/b')

    # Absolute paths.
    self._VerifyNormalized(os.path.join(self.workspace_path, 'a'), '//a')
    self._VerifyNormalized(os.path.join(self.workspace_path, 'a', 'b'), '//a/b')

    # Locators should be unchanged.
    self._VerifyNormalized('//a', '//a')
    self._VerifyNormalized('//a/b', '//a/b')

    # Paths outside the workspace should fail.
    for path in ('/', '..'):
      with self.assertRaises(ValueError):
        commandline.NormalizeWorkspacePath(path)

  def testDefaultDir(self):
    """Tests the default_dir parameter."""
    self._VerifyNormalized('a', '//default/a', default_dir='//default')
    self._VerifyNormalized('a/b', '//a/b', default_dir='//default')
    self._VerifyNormalized('./a', '//a', default_dir='//default')

  def testExtension(self):
    """Tests the extension parameter."""
    self._VerifyNormalized('a', '//a.txt', extension='txt')
    self._VerifyNormalized('a.bin', '//a.bin.txt', extension='txt')
    self._VerifyNormalized('a.txt', '//a.txt', extension='txt')

  def testSpecificPaths(self):
    """Tests normalizing brick/BSP/blueprint paths."""
    self.assertEqual('//bricks/a', commandline.NormalizeBrickPath('a'))
    self.assertEqual('//bsps/a', commandline.NormalizeBspPath('a'))
    self.assertEqual('//blueprints/a.json',
                     commandline.NormalizeBlueprintPath('a'))

  def testParser(self):
    """Tests adding these types to a parser."""
    parser = commandline.ArgumentParser()
    parser.add_argument('path', type='workspace_path')
    parser.add_argument('brick', type='brick_path')
    parser.add_argument('bsp', type='bsp_path')
    parser.add_argument('blueprint', type='blueprint_path')

    options = parser.parse_args(['my_path', 'my_brick', 'my_bsp',
                                 'my_blueprint'])
    self.assertEqual('//my_path', options.path)
    self.assertEqual('//bricks/my_brick', options.brick)
    self.assertEqual('//bsps/my_bsp', options.bsp)
    self.assertEqual('//blueprints/my_blueprint.json', options.blueprint)


class CacheTest(cros_test_lib.MockTempDirTestCase):
  """Test cache dir default / override functionality."""

  CACHE_DIR = '/fake/cache/dir'

  def setUp(self):
    self.PatchObject(commandline.ArgumentParser, 'ConfigureCacheDir')
    dir_struct = [
        'repo/.repo/',
    ]
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, dir_struct)
    self.repo_root = os.path.join(self.tempdir, 'repo')
    self.cwd_mock = self.PatchObject(os, 'getcwd')
    self.parser = commandline.ArgumentParser(caching=True)

  def _CheckCall(self, cwd_retval, args_to_parse, expected, assert_func):
    # pylint: disable=E1101
    self.cwd_mock.return_value = cwd_retval
    self.parser.parse_args(args_to_parse)
    cache_dir_mock = self.parser.ConfigureCacheDir
    self.assertEquals(1, cache_dir_mock.call_count)
    assert_func(cache_dir_mock.call_args[0][0], expected)

  def testRepoRootNoOverride(self):
    """Test default cache location when in a repo checkout."""
    self._CheckCall(self.repo_root, [], self.repo_root, self.assertStartsWith)

  def testRepoRootWithOverride(self):
    """User provided cache location overrides repo checkout default."""
    self._CheckCall(self.repo_root, ['--cache-dir', self.CACHE_DIR],
                    self.CACHE_DIR, self.assertEquals)


class ParseArgsTest(cros_test_lib.TestCase):
  """Test parse_args behavior of our custom argument parsing classes."""

  def _CreateOptionParser(self, cls):
    """Create a class of optparse.OptionParser with prepared config.

    Args:
      cls: Some subclass of optparse.OptionParser.

    Returns:
      The created OptionParser object.
    """
    usage = 'usage: some usage'
    parser = cls(usage=usage)

    # Add some options.
    parser.add_option('-x', '--xxx', action='store_true', default=False,
                      help='Gimme an X')
    parser.add_option('-y', '--yyy', action='store_true', default=False,
                      help='Gimme a Y')
    parser.add_option('-a', '--aaa', type='string', default='Allan',
                      help='Gimme an A')
    parser.add_option('-b', '--bbb', type='string', default='Barry',
                      help='Gimme a B')
    parser.add_option('-c', '--ccc', type='string', default='Connor',
                      help='Gimme a C')

    return parser

  def _CreateArgumentParser(self, cls):
    """Create a class of argparse.ArgumentParser with prepared config.

    Args:
      cls: Some subclass of argparse.ArgumentParser.

    Returns:
      The created ArgumentParser object.
    """
    usage = 'usage: some usage'
    parser = cls(usage=usage)

    # Add some options.
    parser.add_argument('-x', '--xxx', action='store_true', default=False,
                        help='Gimme an X')
    parser.add_argument('-y', '--yyy', action='store_true', default=False,
                        help='Gimme a Y')
    parser.add_argument('-a', '--aaa', type=str, default='Allan',
                        help='Gimme an A')
    parser.add_argument('-b', '--bbb', type=str, default='Barry',
                        help='Gimme a B')
    parser.add_argument('-c', '--ccc', type=str, default='Connor',
                        help='Gimme a C')
    parser.add_argument('args', type=str, nargs='*', help='args')

    return parser

  def _TestParser(self, parser):
    """Test the given parser with a prepared argv."""
    argv = ['-x', '--bbb', 'Bobby', '-c', 'Connor', 'foobar']

    parsed = parser.parse_args(argv)

    if isinstance(parser, commandline.FilteringParser):
      # optparse returns options and args separately.
      options, args = parsed
      self.assertEquals(['foobar'], args)
    else:
      # argparse returns just options.  Options configured above to have the
      # args stored at option "args".
      options = parsed
      self.assertEquals(['foobar'], parsed.args)

    self.assertTrue(options.xxx)
    self.assertFalse(options.yyy)

    self.assertEquals('Allan', options.aaa)
    self.assertEquals('Bobby', options.bbb)
    self.assertEquals('Connor', options.ccc)

    self.assertRaises(AttributeError, getattr, options, 'xyz')

    # Now try altering option values.
    options.aaa = 'Arick'
    self.assertEquals('Arick', options.aaa)

    # Now freeze the options and try altering again.
    options.Freeze()
    self.assertRaises(commandline.cros_build_lib.AttributeFrozenError,
                      setattr, options, 'aaa', 'Arnold')
    self.assertEquals('Arick', options.aaa)

  def testFilterParser(self):
    self._TestParser(self._CreateOptionParser(commandline.FilteringParser))

  def testArgumentParser(self):
    self._TestParser(self._CreateArgumentParser(commandline.ArgumentParser))


class ScriptWrapperMainTest(cros_test_lib.MockTestCase):
  """Test the behavior of the ScriptWrapperMain function."""

  def setUp(self):
    self.PatchObject(sys, 'exit')
    self.lastTargetFound = None

  SYS_ARGV = ['/cmd', '/cmd', 'arg1', 'arg2']
  CMD_ARGS = ['/cmd', 'arg1', 'arg2']
  CHROOT_ARGS = ['--workspace', '/work']

  def testRestartInChrootPreserveArgs(self):
    """Verify args to ScriptWrapperMain are passed through to chroot.."""
    # Setup Mocks/Fakes
    rc = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc.SetDefaultCmdResult()

    def findTarget(target):
      """ScriptWrapperMain needs a function to find a function to run."""
      def raiseChrootRequiredError(args):
        raise commandline.ChrootRequiredError(args)

      self.lastTargetFound = target
      return raiseChrootRequiredError

    # Run Test
    commandline.ScriptWrapperMain(findTarget, self.SYS_ARGV)

    # Verify Results
    rc.assertCommandContains(enter_chroot=True)
    rc.assertCommandContains(self.CMD_ARGS)
    self.assertEqual('/cmd', self.lastTargetFound)

  def testRestartInChrootWithChrootArgs(self):
    """Verify args and chroot args from exception are used."""
    # Setup Mocks/Fakes
    rc = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc.SetDefaultCmdResult()

    def findTarget(_):
      """ScriptWrapperMain needs a function to find a function to run."""
      def raiseChrootRequiredError(_args):
        raise commandline.ChrootRequiredError(self.CMD_ARGS, self.CHROOT_ARGS)

      return raiseChrootRequiredError

    # Run Test
    commandline.ScriptWrapperMain(findTarget, ['unrelated'])

    # Verify Results
    rc.assertCommandContains(enter_chroot=True)
    rc.assertCommandContains(self.CMD_ARGS)
    rc.assertCommandContains(chroot_args=self.CHROOT_ARGS)


class TestRunInsideChroot(cros_test_lib.MockTestCase):
  """Test commandline.RunInsideChroot()."""

  def setUp(self):
    self.orig_argv = sys.argv
    sys.argv = ['/cmd', 'arg1', 'arg2']

    self.mockFromHostToChrootPath = self.PatchObject(
        path_util, 'ToChrootPath', return_value='/inside/cmd')

    # Return values for these two should be set by each test.
    self.mock_inside_chroot = self.PatchObject(cros_build_lib, 'IsInsideChroot')
    self.mock_workspace_path = self.PatchObject(workspace_lib, 'WorkspacePath')

    # Mocked CliCommand object to pass to RunInsideChroot.
    self.cmd = command.CliCommand(argparse.Namespace())
    self.cmd.options.log_level = 'info'

  def teardown(self):
    sys.argv = self.orig_argv

  def _VerifyRunInsideChroot(self, expected_cmd, expected_chroot_args=None,
                             expected_extra_env=None, log_level_args=None,
                             **kwargs):
    """Run RunInsideChroot, and verify it raises with expected values.

    Args:
      expected_cmd: Command that should be executed inside the chroot.
      expected_chroot_args: Args that should be passed as chroot args.
      expected_extra_env: Environmental variables to set in the chroot.
      log_level_args: Args that set the log level of cros_sdk.
      kwargs: Additional args to pass to RunInsideChroot().
    """
    with self.assertRaises(commandline.ChrootRequiredError) as cm:
      commandline.RunInsideChroot(self.cmd, **kwargs)

    if log_level_args is None:
      log_level_args = ['--log-level', self.cmd.options.log_level]

    if expected_chroot_args is not None:
      log_level_args.extend(expected_chroot_args)
      expected_chroot_args = log_level_args
    else:
      expected_chroot_args = log_level_args

    self.assertEqual(expected_cmd, cm.exception.cmd)
    self.assertEqual(expected_chroot_args, cm.exception.chroot_args)
    self.assertEqual(expected_extra_env or {}, cm.exception.extra_env)

  def testRunInsideChrootLogLevel(self):
    self.cmd.options.log_level = 'notice'
    self.mock_inside_chroot.return_value = False
    self.mock_workspace_path.return_value = None
    self._VerifyRunInsideChroot(['/inside/cmd', 'arg1', 'arg2'],
                                log_level_args=['--log-level', 'notice'])

  def testRunInsideChrootNoWorkspace(self):
    """Test we can restart inside the chroot, with no workspace."""
    self.mock_inside_chroot.return_value = False
    self.mock_workspace_path.return_value = None

    self._VerifyRunInsideChroot(['/inside/cmd', 'arg1', 'arg2'])

  def testRunInsideChrootWithWorkspace(self):
    """Test we can restart inside the chroot, with a workspace."""
    self.mock_inside_chroot.return_value = False
    self.mock_workspace_path.return_value = '/work'
    self.PatchObject(path_util.ChrootPathResolver, 'ToChroot',
                     return_value=constants.CHROOT_WORKSPACE_ROOT)

    self._VerifyRunInsideChroot(
        ['/inside/cmd', 'arg1', 'arg2'],
        ['--chroot', '/work/.chroot', '--workspace', '/work'],
        {commandline.CHROOT_CWD_ENV_VAR: constants.CHROOT_WORKSPACE_ROOT})

  def testRunInsideChrootAlreadyInside(self):
    """Test we don't restart inside the chroot if we are already there."""
    self.mock_inside_chroot.return_value = True

    # Since we are in the chroot, it should return, doing nothing.
    commandline.RunInsideChroot(self.cmd)
