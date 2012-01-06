#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import errno
import shutil
import signal
import subprocess
import tempfile
import unittest
import cros_build_lib
import cros_test_lib
import mox

import __builtin__

# pylint: disable=W0212,R0904

class TestRunCommandNoMock(unittest.TestCase):
  """Class that tests RunCommand by not mocking subprocess.Popen"""

  def testErrorCodeNotRaisesError(self):
    """Don't raise exception when command returns non-zero exit code."""
    result = cros_build_lib.RunCommand(['ls', '/does/not/exist'],
                                        error_code_ok=True)
    self.assertTrue(result.returncode != 0)

  def testReturnCodeNotZeroErrorOkNotRaisesError(self):
    """Raise error when proc.communicate() returns non-zero."""
    self.assertRaises(cros_build_lib.RunCommandError, cros_build_lib.RunCommand,
                      ['/does/not/exist'])


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
    for attr in 'close_fds cwd env stdin stdout stderr shell'.split():
      if attr in sp_kv:
        arg_dict[attr] = sp_kv[attr]
      else:
        if attr == 'close_fds':
          arg_dict[attr] = True
        elif attr == 'shell':
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
    self._TestCmd(cmd_list, cmd_list,
                  rc_kv=dict(exit_code=True, ignore_sigint=ignore_sigint))

  def testSignalRestoreNormalCase(self):
    """Test RunCommand() properly sets/restores sigint.  Normal case."""
    self.testReturnCodeZeroWithArrayCmd(ignore_sigint=True)


  def testReturnCodeZeroWithArrayCmdEnterChroot(self):
    """--enter_chroot=True and --cmd is an array of strings."""
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']
    real_cmd = ['cros_sdk', '--'] + cmd_list
    self._TestCmd(cmd_list, real_cmd, rc_kv=dict(exit_code=True,
                                                 enter_chroot=True))



  def testCommandFailureRaisesError(self, ignore_sigint=False):
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

    subprocess.Popen(['/bin/bash', '-c', cmd ], cwd=None, env=None,
                     stdin=None, stdout=None, stderr=None,
                     shell=False, close_fds=True).AndReturn(self.proc_mock)
    self.proc_mock.communicate(None).AndReturn((self.output, self.error))

    # If it ignored them, RunCommand will restore sigints; record that.
    if ignore_sigint:
      signal.signal(signal.SIGINT, self._old_sigint).AndReturn(signal.SIG_IGN)

    self.mox.ReplayAll()
    self.assertRaises(cros_build_lib.RunCommandError,
                      cros_build_lib.RunCommand, cmd, shell=True,
                      ignore_sigint=ignore_sigint, error_ok=False)
    self.mox.VerifyAll()

  def testSubprocessCommunicateExceptionRaisesError(self, ignore_sigint=False):
    """Verify error raised by communicate() is caught.

    Parameterized so this can also be used by some other tests w/ alternate
    params to RunCommand().

    Args:
      ignore_sigint: If True, we'll tell RunCommand to ignore sigint.
    """
    cmd = ['test', 'cmd']

    # If requested, RunCommand will ignore sigints; record that.
    if ignore_sigint:
      signal.signal(signal.SIGINT, signal.SIG_IGN).AndReturn(self._old_sigint)

    subprocess.Popen(cmd, cwd=None, env=None,
                     stdin=None, stdout=None, stderr=None,
                     shell=False, close_fds=True).AndReturn(self.proc_mock)
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
    cmd = ['test', 'cmd']
    real_cmd = ['cros_sdk', '--'] + cmd
    expected_result = cros_build_lib.CommandResult()
    expected_result.cmd = real_cmd

    subprocess.Popen(real_cmd, cwd=None, env=None,
                     stdin=None, stdout=None, stderr=None,
                     shell=False, close_fds=True).AndReturn(self.proc_mock)
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

  def testExtraEnvOnlyWorks(self):
    """Test RunCommand(..., extra_env=xyz) works."""
    # We'll put this bogus environment together, just to make sure
    # subprocess.Popen gets passed it.
    extra_env = {'Pinky' : 'Brain'}
    ## This is a little bit circular, since the same logic is used to compute
    ## the value inside, but at least it checks that this happens.
    total_env = os.environ.copy()
    total_env.update(extra_env)

    # This is a simple case, copied from testReturnCodeZeroWithArrayCmd()
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']

    # Run.  We expect the env= to be passed through from sp (subprocess.Popen)
    # to rc (RunCommand).
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv=dict(env=total_env),
                  rc_kv=dict(extra_env=extra_env, exit_code=True))

  def testExtraEnvTooWorks(self):
    """Test RunCommand(..., env=xy, extra_env=z) works."""
    # We'll put this bogus environment together, just to make sure
    # subprocess.Popen gets passed it.
    env = {'Tom': 'Jerry', 'Itchy': 'Scratchy'}
    extra_env = {'Pinky': 'Brain'}
    total_env = {'Tom': 'Jerry', 'Itchy': 'Scratchy', 'Pinky': 'Brain'}

    # This is a simple case, copied from testReturnCodeZeroWithArrayCmd()
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']

    # Run.  We expect the env= to be passed through from sp (subprocess.Popen)
    # to rc (RunCommand).
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv=dict(env=total_env),
                  rc_kv=dict(env=env, extra_env=extra_env, exit_code=True))



  def testExceptionEquality(self):
    """Verify equality methods for RunCommandError"""

    e1 = cros_build_lib.RunCommandError('Message 1', ['ls', 'arg'], 1)
    e2 = cros_build_lib.RunCommandError('Message 1', ['ls', 'arg'], 1)
    e_diff_msg = cros_build_lib.RunCommandError('Message 2', ['ls', 'arg'], 1)
    e_diff_cmd = cros_build_lib.RunCommandError('Message 1', ['ls', 'arg1'], 1)
    e_diff_code = cros_build_lib.RunCommandError('Message 1', ['ls', 'arg'], 2)

    self.assertTrue(e1 == e2)
    self.assertFalse(e1 != e2)

    self.assertFalse(e1 == e_diff_msg)
    self.assertTrue(e1 != e_diff_msg)

    self.assertFalse(e1 == e_diff_cmd)
    self.assertTrue(e1 != e_diff_cmd)

    self.assertFalse(e1 == e_diff_code)
    self.assertTrue(e1 != e_diff_code)


class TestRunCommandWithRetries(unittest.TestCase):

  @cros_test_lib.tempdir_decorator
  def testBasicRetry(self):
    # pylint: disable=E1101
    path = os.path.join(self.tempdir, 'script')
    stop_path = os.path.join(self.tempdir, 'stop')
    store_path = os.path.join(self.tempdir, 'store')
    with open(path, 'w') as f:
      f.write("import sys\n"
              "val = int(open(%(store)r).read())\n"
              "stop_val = int(open(%(stop)r).read())\n"
              "open(%(store)r, 'w').write(str(val + 1))\n"
              "print val\n"
              "sys.exit(0 if val == stop_val else 1)\n" %
              {'store': store_path, 'stop': stop_path})

    os.chmod(path, 0755)

    def _setup_counters(start, stop):
      with open(store_path, 'w') as f:
        f.write(str(start))

      with open(stop_path, 'w') as f:
        f.write(str(stop))


    _setup_counters(0, 0)
    command = ['python', path]
    kwds = {'redirect_stdout': True, 'print_cmd': False}

    self.assertEqual(cros_build_lib.RunCommand(command, **kwds).output, '0\n')

    func = cros_build_lib.RunCommandWithRetries

    _setup_counters(2, 2)
    self.assertEqual(func(0, command, **kwds).output, '2\n')

    _setup_counters(0, 2)
    self.assertEqual(func(2, command, **kwds).output, '2\n')

    _setup_counters(0, 1)
    self.assertEqual(func(1, command, **kwds).output, '1\n')

    _setup_counters(0, 3)
    self.assertRaises(cros_build_lib.RunCommandError,
                      func, 2, command, **kwds)


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


class YNInteraction():
  """Class to hold a list of responses and expected reault of YN prompt."""
  def __init__(self, responses, expected_result):
    self.responses = responses
    self.expected_result = expected_result


class InputTest(cros_test_lib.MoxTestCase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testGetInput(self):
    self.mox.StubOutWithMock(__builtin__, 'raw_input')

    prompt = 'Some prompt'
    response = 'Some response'
    __builtin__.raw_input(prompt).AndReturn(response)
    self.mox.ReplayAll()

    self.assertEquals(response, cros_build_lib.GetInput(prompt))
    self.mox.VerifyAll()

  def _TestYesNoPrompt(self, interactions, prompt_suffix, default, full):
    self.mox.StubOutWithMock(__builtin__, 'raw_input')

    prompt = 'Continue' # Not important
    actual_prompt = '\n%s %s? ' % (prompt, prompt_suffix)

    for interaction in interactions:
      for response in interaction.responses:
        __builtin__.raw_input(actual_prompt).AndReturn(response)
    self.mox.ReplayAll()

    for interaction in interactions:
      retval = cros_build_lib.YesNoPrompt(default, prompt=prompt, full=full)
      msg = ('A %s prompt with %r responses expected result %r, but got %r' %
             (prompt_suffix, interaction.responses,
              interaction.expected_result, retval))
      self.assertEquals(interaction.expected_result, retval, msg=msg)
    self.mox.VerifyAll()

  def testYesNoDefaultNo(self):
    tests = [YNInteraction([''], cros_build_lib.NO),
             YNInteraction(['n'], cros_build_lib.NO),
             YNInteraction(['no'], cros_build_lib.NO),
             YNInteraction(['foo', ''], cros_build_lib.NO),
             YNInteraction(['foo', 'no'], cros_build_lib.NO),
             YNInteraction(['y'], cros_build_lib.YES),
             YNInteraction(['ye'], cros_build_lib.YES),
             YNInteraction(['yes'], cros_build_lib.YES),
             YNInteraction(['foo', 'yes'], cros_build_lib.YES),
             YNInteraction(['foo', 'bar', 'x', 'y'], cros_build_lib.YES),
             ]
    self._TestYesNoPrompt(tests, '(y/N)', cros_build_lib.NO, False)

  def testYesNoDefaultYes(self):
    tests = [YNInteraction([''], cros_build_lib.YES),
             YNInteraction(['n'], cros_build_lib.NO),
             YNInteraction(['no'], cros_build_lib.NO),
             YNInteraction(['foo', ''], cros_build_lib.YES),
             YNInteraction(['foo', 'no'], cros_build_lib.NO),
             YNInteraction(['y'], cros_build_lib.YES),
             YNInteraction(['ye'], cros_build_lib.YES),
             YNInteraction(['yes'], cros_build_lib.YES),
             YNInteraction(['foo', 'yes'], cros_build_lib.YES),
             YNInteraction(['foo', 'bar', 'x', 'y'], cros_build_lib.YES),
             ]
    self._TestYesNoPrompt(tests, '(Y/n)', cros_build_lib.YES, False)

  def testYesNoDefaultNoFull(self):
    tests = [YNInteraction([''], cros_build_lib.NO),
             YNInteraction(['n', ''], cros_build_lib.NO),
             YNInteraction(['no'], cros_build_lib.NO),
             YNInteraction(['foo', ''], cros_build_lib.NO),
             YNInteraction(['foo', 'no'], cros_build_lib.NO),
             YNInteraction(['y', 'yes'], cros_build_lib.YES),
             YNInteraction(['ye', ''], cros_build_lib.NO),
             YNInteraction(['yes'], cros_build_lib.YES),
             YNInteraction(['foo', 'yes'], cros_build_lib.YES),
             YNInteraction(['foo', 'bar', 'y', ''], cros_build_lib.NO),
             YNInteraction(['n', 'y', 'no'], cros_build_lib.NO),
            ]
    self._TestYesNoPrompt(tests, '(yes/No)', cros_build_lib.NO, True)

  def testYesNoDefaultYesFull(self):
    tests = [YNInteraction([''], cros_build_lib.YES),
             YNInteraction(['n', ''], cros_build_lib.YES),
             YNInteraction(['no'], cros_build_lib.NO),
             YNInteraction(['foo', ''], cros_build_lib.YES),
             YNInteraction(['foo', 'no'], cros_build_lib.NO),
             YNInteraction(['y', 'yes'], cros_build_lib.YES),
             YNInteraction(['n', ''], cros_build_lib.YES),
             YNInteraction(['yes'], cros_build_lib.YES),
             YNInteraction(['foo', 'yes'], cros_build_lib.YES),
             YNInteraction(['foo', 'bar', 'n', ''], cros_build_lib.YES),
             YNInteraction(['n', 'y', 'no'], cros_build_lib.NO),
            ]
    self._TestYesNoPrompt(tests, '(Yes/no)', cros_build_lib.YES, True)


if __name__ == '__main__':
  unittest.main()
