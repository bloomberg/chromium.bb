#!/usr/bin/python
#
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import errno
import os
import shutil
import signal
import subprocess
import tempfile
import unittest
import cros_build_lib
import mox


class TestRunCommand(unittest.TestCase):

  def setUp(self):
    # Get the original value for SIGINT so our signal() mock can return the
    # correct thing.
    self._old_sigint = signal.getsignal(signal.SIGINT)

    self.mox = mox.Mox()
    self.mox.StubOutWithMock(subprocess, 'Popen', use_mock_anything=True)
    self.mox.StubOutWithMock(signal, 'signal')
    self.proc_mock = self.mox.CreateMockAnything()
    self.error = 'test error'
    self.output = 'test output'

  def tearDown(self):
    # Unset anything that we set with mox.
    self.mox.UnsetStubs()

  def _AssertCrEqual(self, expected, actual):
    """Helper method to compare two CommandResult objects.

    This is needed since assertEqual does not know how to compare two
    CommandResult objects.

    Args:
      expected: a CommandResult object, expected result.
      actual: a CommandResult object, actual result.
    """
    self.assertEqual(expected.cmd, actual.cmd)
    self.assertEqual(expected.error, actual.error)
    self.assertEqual(expected.output, actual.output)
    self.assertEqual(expected.returncode, actual.returncode)

  def _TestCmd(self, cmd, real_cmd, sp_kv=dict(), rc_kv=dict()):
    """Factor out common setup logic for testing --cmd.

    Args:
      cmd: a string or an array of strings that will be passed to RunCommand.
      real_cmd: the real command we expect RunCommand to call (might be
          modified to have enter_chroot).
      sp_kv: key-value pairs passed to subprocess.Popen().
      rc_kv: key-value pairs passed to RunCommand().
    """
    expected_result = cros_build_lib.CommandResult()
    expected_result.cmd = real_cmd
    expected_result.error = self.error
    expected_result.output = self.output
    if 'exit_code' in rc_kv:
      expected_result.returncode = self.proc_mock.returncode

    arg_dict = dict()
    for attr in 'cwd env stdin stdout stderr shell'.split():
      if attr in sp_kv:
        arg_dict[attr] = sp_kv[attr]
      else:
        if attr == 'shell':
          arg_dict[attr] = False
        else:
          arg_dict[attr] = None

    # If requested, RunCommand will ignore sigints; record that.
    if rc_kv.get('ignore_sigint'):
      signal.signal(signal.SIGINT, signal.SIG_IGN).AndReturn(self._old_sigint)

    subprocess.Popen(real_cmd, **arg_dict).AndReturn(self.proc_mock)
    self.proc_mock.communicate(None).AndReturn((self.output, self.error))

    # If it ignored them, RunCommand will restore sigints; record that.
    if rc_kv.get('ignore_sigint'):
      signal.signal(signal.SIGINT, self._old_sigint).AndReturn(signal.SIG_IGN)

    self.mox.ReplayAll()
    actual_result = cros_build_lib.RunCommand(cmd, **rc_kv)
    self.mox.VerifyAll()

    self._AssertCrEqual(expected_result, actual_result)

  def testReturnCodeZeroWithArrayCmd(self, ignore_sigint=False):
    """--enter_chroot=False and --cmd is an array of strings.

    Parameterized so this can also be used by some other tests w/ alternate
    params to RunCommand().

    Args:
      ignore_sigint: If True, we'll tell RunCommand to ignore sigint.
    """
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']
    self._TestCmd(cmd_list, cmd_list, rc_kv=dict(exit_code=True,
                                                 ignore_sigint=ignore_sigint))

  def testSignalRestoreNormalCase(self):
    """Test RunCommand() properly sets/restores sigint.  Normal case."""
    self.testReturnCodeZeroWithArrayCmd(ignore_sigint=True)


  def testReturnCodeZeroWithArrayCmdEnterChroot(self):
    """--enter_chroot=True and --cmd is an array of strings."""
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']
    real_cmd = ['./enter_chroot.sh', '--'] + cmd_list
    self._TestCmd(cmd_list, real_cmd, rc_kv=dict(enter_chroot=True))

  def testReturnCodeNotZeroErrorOkNotRaisesError(self):
    """Raise error when proc.communicate() returns non-zero."""
    self.proc_mock.returncode = 1
    cmd = 'test cmd'
    self._TestCmd(cmd, cmd, rc_kv=dict(error_ok=True))

  def testSubprocessCommunicateExceptionRaisesError(self, ignore_sigint=False):
    """Verify error raised by communicate() is caught.

    Parameterized so this can also be used by some other tests w/ alternate
    params to RunCommand().

    Args:
      ignore_sigint: If True, we'll tell RunCommand to ignore sigint.
    """
    cmd = 'test cmd'

    # If requested, RunCommand will ignore sigints; record that.
    if ignore_sigint:
      signal.signal(signal.SIGINT, signal.SIG_IGN).AndReturn(self._old_sigint)

    subprocess.Popen(cmd, cwd=None, env=None,
                     stdin=None, stdout=None, stderr=None,
                     shell=False).AndReturn(self.proc_mock)
    self.proc_mock.communicate(None).AndRaise(ValueError)

    # If it ignored them, RunCommand will restore sigints; record that.
    if ignore_sigint:
      signal.signal(signal.SIGINT, self._old_sigint).AndReturn(signal.SIG_IGN)

    self.mox.ReplayAll()
    self.assertRaises(ValueError, cros_build_lib.RunCommand, cmd,
                      ignore_sigint=ignore_sigint)
    self.mox.VerifyAll()

  def testSignalRestoreExceptionCase(self):
    """Test RunCommand() properly sets/restores sigint.  Exception case."""
    self.testSubprocessCommunicateExceptionRaisesError(ignore_sigint=True)

  def testSubprocessCommunicateExceptionNotRaisesError(self):
    """Don't re-raise error from communicate() when --error_ok=True."""
    cmd = 'test cmd'
    real_cmd = './enter_chroot.sh -- %s' % cmd
    expected_result = cros_build_lib.CommandResult()
    expected_result.cmd = real_cmd

    subprocess.Popen(real_cmd, cwd=None, env=None,
                     stdin=None, stdout=None, stderr=None,
                     shell=False).AndReturn(self.proc_mock)
    self.proc_mock.communicate(None).AndRaise(ValueError)

    self.mox.ReplayAll()
    actual_result = cros_build_lib.RunCommand(cmd, error_ok=True,
                                              enter_chroot=True)
    self.mox.VerifyAll()

    self._AssertCrEqual(expected_result, actual_result)

  def testEnvWorks(self):
    """Test RunCommand(..., env=xyz) works."""
    # We'll put this bogus environment together, just to make sure
    # subprocess.Popen gets passed it.
    env = {'Tom': 'Jerry', 'Itchy': 'Scratchy'}

    # This is a simple case, copied from testReturnCodeZeroWithArrayCmd()
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']

    # Run.  We expect the env= to be passed through from sp (subprocess.Popen)
    # to rc (RunCommand).
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv=dict(env=env),
                  rc_kv=dict(env=env, exit_code=True))


  def testExceptionEquality(self):
    """Verify equality methods for RunCommandError"""

    e1 = cros_build_lib.RunCommandError('Message 1', ['ls', 'arg'])
    e2 = cros_build_lib.RunCommandError('Message 1', ['ls', 'arg'])
    e_diff_msg = cros_build_lib.RunCommandError('Message 2', ['ls', 'arg'])
    e_diff_cmd = cros_build_lib.RunCommandError('Message 1', ['ls', 'arg1'])

    self.assertTrue(e1 == e2)
    self.assertFalse(e1 != e2)

    self.assertFalse(e1 == e_diff_msg)
    self.assertTrue(e1 != e_diff_msg)

    self.assertFalse(e1 == e_diff_cmd)
    self.assertTrue(e1 != e_diff_cmd)

class TestListFiles(unittest.TestCase):

  def setUp(self):
    self.root_dir = tempfile.mkdtemp(prefix='listfiles_unittest')

  def tearDown(self):
    shutil.rmtree(self.root_dir)

  def _CreateNestedDir(self, dir_structure):
    for entry in dir_structure:
      full_path = os.path.join(os.path.join(self.root_dir, entry))
      # ensure dirs are created
      try:
        os.makedirs(os.path.dirname(full_path))
        if full_path.endswith('/'):
          # we only want to create directories
          return
      except OSError, err:
        if err.errno == errno.EEXIST:
          # we don't care if the dir already exists
          pass
        else:
          raise
      # create dummy files
      tmp = open(full_path, 'w')
      tmp.close()

  def testTraverse(self):
    """Test that we are traversing the directory properly."""
    dir_structure = ['one/two/test.txt', 'one/blah.py',
                     'three/extra.conf']
    self._CreateNestedDir(dir_structure)

    files = cros_build_lib.ListFiles(self.root_dir)
    for f in files:
      f = f.replace(self.root_dir, '').lstrip('/')
      if f not in dir_structure:
        self.fail('%s was not found in %s' % (f, dir_structure))

  def testEmptyFilePath(self):
    """Test that we return nothing when directories are empty."""
    dir_structure = ['one/', 'two/', 'one/a/']
    self._CreateNestedDir(dir_structure)
    files = cros_build_lib.ListFiles(self.root_dir)
    self.assertEqual(files, [])

  def testNoSuchDir(self):
    try:
      cros_build_lib.ListFiles('/me/no/existe')
    except OSError, err:
      self.assertEqual(err.errno, errno.ENOENT)

class TestOldRunCommand(unittest.TestCase):
  """Tests related to OldRunCommand."""

  def testExceptionEquality(self):
    """Verify equality methods for RunCommandException"""
    e1 = cros_build_lib.RunCommandException('Message 1', ['ls', 'arg'])
    e2 = cros_build_lib.RunCommandException('Message 1', ['ls', 'arg'])
    e_diff_msg = cros_build_lib.RunCommandException('Message 2', ['ls', 'arg'])
    e_diff_cmd = cros_build_lib.RunCommandException('Message 1', ['ls', 'arg1'])

    self.assertTrue(e1 == e2)
    self.assertFalse(e1 != e2)

    self.assertFalse(e1 == e_diff_msg)
    self.assertTrue(e1 != e_diff_msg)

    self.assertFalse(e1 == e_diff_cmd)
    self.assertTrue(e1 != e_diff_cmd)


class HelperMethodMoxTests(unittest.TestCase):
  """Tests for various helper methods using mox."""

  def setUp(self):
    self.mox = mox.Mox()
    self.mox.StubOutWithMock(os.path, 'abspath')

  def tearDown(self):
    self.mox.UnsetStubs()
    self.mox.VerifyAll()

  def testGetSrcRoot(self):
    test_path = '/tmp/foo/src/scripts/bar/more'
    expected = '/tmp/foo/src/scripts'
    os.path.abspath('.').AndReturn(test_path)
    self.mox.ReplayAll()
    actual = cros_build_lib.GetSrcRoot()
    self.assertEqual(expected, actual)

  def testGetOutputImageDir(self):
    expected = '/tmp/foo/src/build/images/x86-generic/0.0.1-a1'
    self.mox.StubOutWithMock(cros_build_lib, 'GetSrcRoot')
    cros_build_lib.GetSrcRoot().AndReturn('/tmp/foo/src/scripts')
    self.mox.ReplayAll()
    actual = cros_build_lib.GetOutputImageDir('x86-generic', '0.0.1')
    self.assertEqual(expected, actual)


class HelperMethodSimpleTests(unittest.TestCase):
  """Tests for various helper methods without using mox."""

  def _TestChromeosVersion(self, test_str, expected=None):
    actual = cros_build_lib.GetChromeosVersion(test_str)
    self.assertEqual(expected, actual)

  def testGetChromeosVersionWithValidVersionReturnsValue(self):
    expected = '0.8.71.2010_09_10_1530'
    test_str = ' CHROMEOS_VERSION_STRING=0.8.71.2010_09_10_1530 '
    self._TestChromeosVersion(test_str, expected)

  def testGetChromeosVersionWithMultipleVersionReturnsFirstMatch(self):
    expected = '0.8.71.2010_09_10_1530'
    test_str = (' CHROMEOS_VERSION_STRING=0.8.71.2010_09_10_1530 '
                ' CHROMEOS_VERSION_STRING=10_1530 ')
    self._TestChromeosVersion(test_str, expected)

  def testGetChromeosVersionWithInvalidVersionReturnsDefault(self):
    test_str = ' CHROMEOS_VERSION_STRING=invalid_version_string '
    self._TestChromeosVersion(test_str)

  def testGetChromeosVersionWithEmptyInputReturnsDefault(self):
    self._TestChromeosVersion('')

  def testGetChromeosVersionWithNoneInputReturnsDefault(self):
    self._TestChromeosVersion(None)


if __name__ == '__main__':
  unittest.main()
