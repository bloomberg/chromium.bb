# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the commandline module."""

from __future__ import print_function

import cPickle
import signal
import os
import sys

from chromite.lib import commandline
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import partial_mock

from chromite.cbuildbot import constants

# pylint: disable=W0212
class TestShutDownException(cros_test_lib.TestCase):
  """Test that ShutDownException can be pickled."""

  def testShutDownException(self):
    """Test that ShutDownException can be pickled."""
    ex = commandline._ShutDownException(signal.SIGTERM, 'Received SIGTERM')
    ex2 = cPickle.loads(cPickle.dumps(ex))
    self.assertEqual(ex.signal, ex2.signal)
    self.assertEqual(ex.message, ex2.message)


class GSPathTest(cros_test_lib.TestCase):
  """Test type=gs_path normalization functionality."""

  GS_REL_PATH = 'bucket/path/to/artifacts'

  @staticmethod
  def _ParseCommandLine(argv):
    parser = commandline.OptionParser()
    parser.add_option('-g', '--gs-path', type='gs_path',
                      help=('GS path that contains the chrome to deploy.'))
    return parser.parse_args(argv)

  def _RunGSPathTestCase(self, raw, parsed):
    options, _ = self._ParseCommandLine(['--gs-path', raw])
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
    with cros_test_lib.OutputCapturer():
      self.assertRaises2(
          SystemExit, self._RunGSPathTestCase, 'http://badhost.com/path', '',
          check_attrs={'code': 2})


class DeviceParseTest(cros_test_lib.TestCase):
  """Test device parsing functionality."""

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
    parser.add_argument('device', type='device')
    device = parser.parse_args([device_input]).device
    self.assertEqual(device.scheme, scheme)
    self.assertEqual(device.username, username)
    self.assertEqual(device.hostname, hostname)
    self.assertEqual(device.port, port)
    self.assertEqual(device.path, path)

  def _CheckDeviceParseFails(self, device_input):
    """Checks that parsing a device input fails.

    Args:
      device_input: String input specifying a device.
    """
    parser = commandline.ArgumentParser()
    parser.add_argument('device', type='device')
    with cros_test_lib.OutputCapturer():
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
    self._CheckDeviceParseFails('ftp://192.168.1.200')

  def testSchemeCaseInsensitive(self):
    """Verify that schemes are case-insensitive."""
    self._CheckDeviceParse('SSH://foo_host',
                           scheme=commandline.DEVICE_SCHEME_SSH,
                           hostname='foo_host')


class DetermineCheckoutTest(cros_test_lib.MockTempDirTestCase):
  """Verify functionality for figuring out what checkout we're in."""

  def setUp(self):
    self.rc_mock = cros_build_lib_unittest.RunCommandMock()
    self.StartPatcher(self.rc_mock)
    self.rc_mock.SetDefaultCmdResult()

  def RunTest(self, dir_struct, cwd, expected_root, expected_type,
              expected_src):
    """Run a test with specific parameters and expected results."""
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, dir_struct)
    cwd = os.path.join(self.tempdir, cwd)
    checkout_info = commandline.DetermineCheckout(cwd)
    full_root = expected_root
    if expected_root is not None:
      full_root = os.path.join(self.tempdir, expected_root)
    full_src = expected_src
    if expected_src is not None:
      full_src = os.path.join(self.tempdir, expected_src)

    self.assertEquals(checkout_info.root, full_root)
    self.assertEquals(checkout_info.type, expected_type)
    self.assertEquals(checkout_info.chrome_src_dir, full_src)

  def testGclientRepo(self):
    dir_struct = [
        'a/.gclient',
        'a/b/.repo/',
        'a/b/c/.gclient',
        'a/b/c/d/somefile',
    ]
    self.RunTest(dir_struct, 'a/b/c', 'a/b/c',
                 commandline.CHECKOUT_TYPE_GCLIENT,
                 'a/b/c/src')
    self.RunTest(dir_struct, 'a/b/c/d', 'a/b/c',
                 commandline.CHECKOUT_TYPE_GCLIENT,
                 'a/b/c/src')
    self.RunTest(dir_struct, 'a/b', 'a/b',
                 commandline.CHECKOUT_TYPE_REPO,
                 None)
    self.RunTest(dir_struct, 'a', 'a',
                 commandline.CHECKOUT_TYPE_GCLIENT,
                 'a/src')

  def testGitSubmodule(self):
    """Recognizes a chrome git submodule checkout."""
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMIUM_GOB_URL)
    dir_struct = [
        'a/.gclient',
        'a/.repo',
        'a/b/.git/',
    ]
    self.RunTest(dir_struct, 'a/b', 'a/b',
                 commandline.CHECKOUT_TYPE_SUBMODULE,
                 'a/b')

  def testBadGit1(self):
    """.git is not a directory."""
    self.RunTest(['a/.git'], 'a', None,
                 commandline.CHECKOUT_TYPE_UNKNOWN, None)

  def testBadGit2(self):
    """'git config' returns nothing."""
    self.RunTest(['a/.repo/', 'a/b/.git/'], 'a/b', 'a',
                 commandline.CHECKOUT_TYPE_REPO, None)

  def testBadGit3(self):
    """'git config' returns error."""
    self.rc_mock.AddCmdResult(partial_mock.In('config'), returncode=5)
    self.RunTest(['a/.git/'], 'a', None,
                 commandline.CHECKOUT_TYPE_UNKNOWN, None)


class CacheTest(cros_test_lib.MockTempDirTestCase):
  """Test cache dir specification and finding functionality."""

  REPO_ROOT = '/fake/repo/root'
  GCLIENT_ROOT = '/fake/gclient/root'
  SUBMODULE_ROOT = '/fake/submodule/root'
  CACHE_DIR = '/fake/cache/dir'

  def setUp(self):
    self.PatchObject(commandline.ArgumentParser, 'ConfigureCacheDir')
    dir_struct = [
        'repo/.repo/',
        'gclient/.gclient',
        'submodule/.git/',
    ]
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, dir_struct)
    self.repo_root = os.path.join(self.tempdir, 'repo')
    self.gclient_root = os.path.join(self.tempdir, 'gclient')
    self.submodule_root = os.path.join(self.tempdir, 'submodule')
    self.nocheckout_root = os.path.join(self.tempdir, 'nothing')

    self.rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    self.rc_mock.AddCmdResult(
        partial_mock.In('config'), output=constants.CHROMIUM_GOB_URL)
    self.cwd_mock = self.PatchObject(os, 'getcwd')
    self.parser = commandline.ArgumentParser(caching=True)

  def _CheckCall(self, expected):
    # pylint: disable=E1101
    f = self.parser.ConfigureCacheDir
    self.assertEquals(1, f.call_count)
    self.assertTrue(f.call_args[0][0].startswith(expected))

  def testRepoRoot(self):
    """Test when we are inside a repo checkout."""
    self.cwd_mock.return_value = self.repo_root
    self.parser.parse_args([])
    self._CheckCall(self.repo_root)

  def testGclientRoot(self):
    """Test when we are inside a gclient checkout."""
    self.cwd_mock.return_value = self.gclient_root
    self.parser.parse_args([])
    self._CheckCall(self.gclient_root)

  def testSubmoduleRoot(self):
    """Test when we are inside a git submodule Chrome checkout."""
    self.cwd_mock.return_value = self.submodule_root
    self.parser.parse_args([])
    self._CheckCall(self.submodule_root)

  def testTempdir(self):
    """Test when we are not in any checkout."""
    self.cwd_mock.return_value = self.nocheckout_root
    self.parser.parse_args([])
    self._CheckCall('/tmp')

  def testSpecifiedDir(self):
    """Test when user specifies a cache dir."""
    self.cwd_mock.return_value = self.repo_root
    self.parser.parse_args(['--cache-dir', self.CACHE_DIR])
    self._CheckCall(self.CACHE_DIR)


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

    if isinstance(parser, commandline.OptionParser):
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

  def testOptionParser(self):
    self._TestParser(self._CreateOptionParser(commandline.OptionParser))

  def testFilterParser(self):
    self._TestParser(self._CreateOptionParser(commandline.FilteringParser))

  def testArgumentParser(self):
    self._TestParser(self._CreateArgumentParser(commandline.ArgumentParser))


class ScriptWrapperMainTest(cros_test_lib.MockTestCase):
  """Test the behavior of the ScriptWrapperMain function."""

  def setUp(self):
    self.PatchObject(sys, 'exit')

  # pylint: disable=W0613
  @staticmethod
  def _DummyChrootTarget(args):
    raise commandline.ChrootRequiredError()

  DUMMY_CHROOT_TARGET_ARGS = ['cmd', 'arg1', 'arg2']

  DUMMY_EXTRA_ARGS = ['extra', 'arg']

  @staticmethod
  def _DummyChrootTargetArgs(args):
    args = ScriptWrapperMainTest.DUMMY_CHROOT_TARGET_ARGS
    raise commandline.ChrootRequiredError(args)

  @staticmethod
  def _DummyChrootTargetExtraArgs(args):
    extra_args = ScriptWrapperMainTest.DUMMY_EXTRA_ARGS
    raise commandline.ChrootRequiredError(extra_args=extra_args)

  def testRestartInChroot(self):
    rc = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc.SetDefaultCmdResult()

    ret = lambda x: ScriptWrapperMainTest._DummyChrootTarget
    commandline.ScriptWrapperMain(ret)
    rc.assertCommandContains(enter_chroot=True)
    rc.assertCommandContains(self.DUMMY_CHROOT_TARGET_ARGS, expected=False)

  def testRestartInChrootArgs(self):
    rc = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc.SetDefaultCmdResult()

    ret = lambda x: ScriptWrapperMainTest._DummyChrootTargetArgs
    commandline.ScriptWrapperMain(ret)
    rc.assertCommandContains(self.DUMMY_CHROOT_TARGET_ARGS, enter_chroot=True)

  def testRestartInChrootExtraArgs(self):
    rc = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc.SetDefaultCmdResult()

    ret = lambda x: ScriptWrapperMainTest._DummyChrootTargetExtraArgs
    commandline.ScriptWrapperMain(ret)
    rc.assertCommandContains(self.DUMMY_EXTRA_ARGS, enter_chroot=True)
