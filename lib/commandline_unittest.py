#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the commandline module."""

import cPickle
import signal
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import commandline
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import partial_mock

from chromite.buildbot import constants

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
    options, _ =  self._ParseCommandLine(['--gs-path', raw])
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


if __name__ == '__main__':
  cros_test_lib.main()
