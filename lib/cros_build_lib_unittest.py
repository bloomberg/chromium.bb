# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the cros_build_lib module."""

from __future__ import print_function

import contextlib
import datetime
import difflib
import functools
import itertools
import os
import signal
import socket
import subprocess
import sys

import mock
from six.moves import builtins

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import signals as cros_signals


class RunCommandErrorStrTest(cros_test_lib.TestCase):
  """Test that RunCommandError __str__ works as expected."""

  def testNonUTF8Characters(self):
    """Test that non-UTF8 characters do not kill __str__"""
    result = cros_build_lib.run(['ls', '/does/not/exist'], error_code_ok=True)
    rce = cros_build_lib.RunCommandError('\x81', result)
    str(rce)


class TruncateStringTest(cros_test_lib.TestCase):
  """Test the TruncateStringToLine function."""

  def testTruncate(self):
    self.assertEqual(cros_build_lib.TruncateStringToLine('1234567', 5),
                     '12...')

  def testNoTruncate(self):
    self.assertEqual(cros_build_lib.TruncateStringToLine('1234567', 7),
                     '1234567')

  def testNoTruncateMultiline(self):
    self.assertEqual(cros_build_lib.TruncateStringToLine('1234567\nasdf', 7),
                     '1234567')


class CmdToStrTest(cros_test_lib.TestCase):
  """Test the CmdToStr function."""

  def setUp(self):
    self.differ = difflib.Differ()

  def _assertEqual(self, func, test_input, test_output, result):
    """Like assertEqual but with built in diff support."""
    msg = ('Expected %s to translate %r to %r, but got %r' %
           (func, test_input, test_output, result))
    self.assertEqual(test_output, result, msg)

  def _testData(self, functor, tests, check_type=True):
    """Process an iterable of test data."""
    for test_output, test_input in tests:
      result = functor(test_input)
      self._assertEqual(functor.__name__, test_input, test_output, result)

      if check_type:
        # Also make sure the result is a string, otherwise the %r output will
        # include a "u" prefix and that is not good for logging.
        self.assertEqual(type(test_output), str)

  def testShellQuote(self):
    """Basic ShellQuote tests."""
    # Tuples of (expected output string, input data).
    tests_quote = (
        ("''", ''),
        ('a', u'a'),
        ("'a b c'", u'a b c'),
        ("'a\tb'", 'a\tb'),
        ("'/a$file'", '/a$file'),
        ("'/a#file'", '/a#file'),
        ("""'b"c'""", 'b"c'),
        ("'a@()b'", 'a@()b'),
        ('j%k', 'j%k'),
        # pylint: disable=invalid-triple-quote
        # https://github.com/edaniszewski/pylint-quotes/issues/20
        (r'''"s'a\$va\\rs"''', r"s'a$va\rs"),
        (r'''"\\'\\\""''', r'''\'\"'''),
        (r'''"'\\\$"''', r"""'\$"""),
    )

    bytes_quote = (
        # Since we allow passing bytes down, quote them too.
        ('bytes', b'bytes'),
        ("'by tes'", b'by tes'),
        ('bytes', u'bytes'),
        ("'by tes'", u'by tes'),
    )

    # Expected input output specific to ShellUnquote. This string cannot be
    # produced by ShellQuote but is still a valid bash escaped string.
    tests_unquote = (
        # pylint: disable=invalid-triple-quote
        # https://github.com/edaniszewski/pylint-quotes/issues/20
        (r'''\$''', r'''"\\$"'''),
    )

    def aux(s):
      return cros_build_lib.ShellUnquote(cros_build_lib.ShellQuote(s))

    if sys.version_info.major < 3:
      # In Python 2, these roundtrip.
      tests_quote += bytes_quote
    else:
      # In Python 3, we can only go one way bytes->string.
      self._testData(cros_build_lib.ShellQuote, bytes_quote)
      self._testData(aux, [(x, x) for x, _ in bytes_quote], False)

    self._testData(cros_build_lib.ShellQuote, tests_quote)
    self._testData(cros_build_lib.ShellUnquote, tests_unquote)

    # Test that the operations are reversible.
    self._testData(aux, [(x, x) for x, _ in tests_quote], False)
    self._testData(aux, [(x, x) for _, x in tests_quote], False)

  def testShellQuoteOjbects(self):
    """Test objects passed to ShellQuote."""
    self.assertEqual('None', cros_build_lib.ShellQuote(None))
    self.assertNotEqual('', cros_build_lib.ShellQuote(object))

  def testCmdToStr(self):
    # Dict of expected output strings to input lists.
    tests = (
        (r'a b', ['a', 'b']),
        (r"'a b' c", ['a b', 'c']),
        # pylint: disable=invalid-triple-quote
        # https://github.com/edaniszewski/pylint-quotes/issues/20
        (r'''a "b'c"''', ['a', "b'c"]),
        (r'''a "/'\$b" 'a b c' "xy'z"''',
         [u'a', "/'$b", 'a b c', "xy'z"]),
        ('', []),
        ('a b c', [b'a', 'b', u'c']),
        ('bad None cmd', ['bad', None, 'cmd']),
    )
    self._testData(cros_build_lib.CmdToStr, tests)


class TestRunCommandNoMock(cros_test_lib.TestCase):
  """Class that tests run by not mocking subprocess.Popen"""

  def testErrorCodeNotRaisesError(self):
    """Don't raise exception when command returns non-zero exit code."""
    result = cros_build_lib.run(['ls', '/does/not/exist'], error_code_ok=True)
    self.assertTrue(result.returncode != 0)

  def testMissingCommandRaisesError(self):
    """Raise error when command is not found."""
    self.assertRaises(cros_build_lib.RunCommandError, cros_build_lib.run,
                      ['/does/not/exist'], error_code_ok=False)
    self.assertRaises(cros_build_lib.RunCommandError, cros_build_lib.run,
                      ['/does/not/exist'], error_code_ok=True)

  def testInputBytes(self):
    """Verify input argument when it is bytes."""
    for data in (b'', b'foo', b'bar\nhigh'):
      result = cros_build_lib.run(['cat'], input=data)
      self.assertEqual(result.stdout, data)

  def testInputBytesEncoding(self):
    """Verify bytes input argument when encoding is set."""
    for data in (b'', b'foo', b'bar\nhigh'):
      result = cros_build_lib.run(['cat'], input=data, encoding='utf-8')
      self.assertEqual(result.stdout, data.decode('utf-8'))

  def testInputString(self):
    """Verify input argument when it is a string."""
    for data in ('', 'foo', 'bar\nhigh'):
      result = cros_build_lib.run(['cat'], input=data)
      self.assertEqual(result.stdout, data.encode('utf-8'))

  def testInputStringEncoding(self):
    """Verify bytes input argument when encoding is set."""
    for data in ('', 'foo', 'bar\nhigh'):
      result = cros_build_lib.run(['cat'], input=data, encoding='utf-8')
      self.assertEqual(result.stdout, data)

  def testInputFileObject(self):
    """Verify input argument when it is a file object."""
    result = cros_build_lib.run(['cat'], input=open('/dev/null'))
    self.assertEqual(result.output, b'')

    with open(__file__) as f:
      result = cros_build_lib.run(['cat'], input=f)
      self.assertEqual(result.stdout,
                       osutils.ReadFile(__file__, mode='rb'))

  def testInputFileDescriptor(self):
    """Verify input argument when it is a file descriptor."""
    with open('/dev/null') as f:
      result = cros_build_lib.run(['cat'], input=f.fileno())
      self.assertEqual(result.output, b'')

    with open(__file__) as f:
      result = cros_build_lib.run(['cat'], input=f.fileno())
      self.assertEqual(result.stdout,
                       osutils.ReadFile(__file__, mode='rb'))

  def testMixedEncodingCommand(self):
    """Verify cmd can mix bytes & strings."""
    result = cros_build_lib.run([b'echo', 'hi', u'ß'], capture_output=True,
                                encoding='utf-8')
    self.assertEqual(result.stdout, u'hi ß\n')

  def testEncodingBinaryOutput(self):
    """Verify encoding=None output handling."""
    result = cros_build_lib.run(b'echo o\xff ut; echo e\xff rr >&2',
                                shell=True, capture_output=True)
    self.assertEqual(result.stdout, b'o\xff ut\n')
    self.assertEqual(result.stderr, b'e\xff rr\n')

  def testEncodingUtf8Output(self):
    """Verify encoding='utf-8' output handling."""
    result = cros_build_lib.run(['echo', u'ß'], capture_output=True,
                                encoding='utf-8')
    self.assertEqual(result.stdout, u'ß\n')

  def testEncodingStrictInvalidUtf8Output(self):
    """Verify encoding='utf-8' output with invalid content."""
    with self.assertRaises(UnicodeDecodeError):
      cros_build_lib.run(['echo', b'\xff'], capture_output=True,
                         encoding='utf-8')
    with self.assertRaises(UnicodeDecodeError):
      cros_build_lib.run(['echo', b'\xff'], capture_output=True,
                         encoding='utf-8', errors='strict')

  def testEncodingReplaceInvalidUtf8Output(self):
    """Verify encoding='utf-8' errors='replace' output with invalid content."""
    result = cros_build_lib.run(['echo', b'S\xffE'], capture_output=True,
                                encoding='utf-8', errors='replace')
    self.assertEqual(result.stdout, u'S\ufffdE\n')


def _ForceLoggingLevel(functor):
  def inner(*args, **kwargs):
    logger = logging.getLogger()
    current = logger.getEffectiveLevel()
    try:
      logger.setLevel(logging.INFO)
      return functor(*args, **kwargs)
    finally:
      logger.setLevel(current)
  return inner


class TestRunCommand(cros_test_lib.MockTestCase):
  """Tests of run functionality."""

  def setUp(self):
    # These ENV variables affect run behavior, hide them.
    self._old_envs = {e: os.environ.pop(e) for e in constants.ENV_PASSTHRU
                      if e in os.environ}

    # Get the original value for SIGINT so our signal() mock can return the
    # correct thing.
    self._old_sigint = signal.getsignal(signal.SIGINT)

    # Mock the return value of Popen().
    self.stdin = None
    self.error = b'test error'
    self.output = b'test output'
    self.proc_mock = mock.MagicMock(
        returncode=0,
        communicate=self._Communicate)
    self.popen_mock = self.PatchObject(cros_build_lib, '_Popen',
                                       return_value=self.proc_mock)

    self.signal_mock = self.PatchObject(signal, 'signal')
    self.getsignal_mock = self.PatchObject(signal, 'getsignal')
    self.PatchObject(cros_signals, 'SignalModuleUsable', return_value=True)

  def tearDown(self):
    # Restore hidden ENVs.
    os.environ.update(self._old_envs)

  def _Communicate(self, stdin):
    """Used by mocked _Popen for communicate method to capture what
    was passed on input.
    """
    self.stdin = stdin
    return self.output, self.error

  @contextlib.contextmanager
  def _MockChecker(self, cmd, **kwargs):
    """Verify the mocks we set up"""
    ignore_sigint = kwargs.pop('ignore_sigint', False)

    # Make some arbitrary functors we can pretend are signal handlers.
    # Note that these are intentionally defined on the fly via lambda-
    # this is to ensure that they're unique to each run.
    sigint_suppress = lambda signum, frame: None
    sigint_suppress.__name__ = 'sig_ign_sigint'
    normal_sigint = lambda signum, frame: None
    normal_sigint.__name__ = 'sigint'
    normal_sigterm = lambda signum, frame: None
    normal_sigterm.__name__ = 'sigterm'

    # Set up complicated mock for signal.signal().
    def _SignalChecker(sig, _action):
      """Return the right signal values so we can check the calls."""
      if sig == signal.SIGINT:
        return sigint_suppress if ignore_sigint else normal_sigint
      elif sig == signal.SIGTERM:
        return normal_sigterm
      else:
        raise ValueError('unknown sig %i' % sig)
    self.signal_mock.side_effect = _SignalChecker

    # Set up complicated mock for signal.getsignal().
    def _GetsignalChecker(sig):
      """Return the right signal values so we can check the calls."""
      if sig == signal.SIGINT:
        self.assertFalse(ignore_sigint)
        return normal_sigint
      elif sig == signal.SIGTERM:
        return normal_sigterm
      else:
        raise ValueError('unknown sig %i' % sig)
    self.getsignal_mock.side_effect = _GetsignalChecker

    # Let the body of code run, then check the signal behavior afterwards.
    # We don't get visibility into signal ordering vs command execution,
    # but it's kind of hard to mess up that, so we won't bother.
    yield

    class RejectSigIgn(object):
      """Make sure the signal action is not SIG_IGN."""
      def __eq__(self, other):
        return other != signal.SIG_IGN

    # Verify the signals checked/setup are correct.
    if ignore_sigint:
      self.signal_mock.assert_has_calls([
          mock.call(signal.SIGINT, signal.SIG_IGN),
          mock.call(signal.SIGTERM, RejectSigIgn()),
          mock.call(signal.SIGINT, sigint_suppress),
          mock.call(signal.SIGTERM, normal_sigterm),
      ])
      self.assertEqual(self.getsignal_mock.call_count, 1)
    else:
      self.signal_mock.assert_has_calls([
          mock.call(signal.SIGINT, RejectSigIgn()),
          mock.call(signal.SIGTERM, RejectSigIgn()),
          mock.call(signal.SIGINT, normal_sigint),
          mock.call(signal.SIGTERM, normal_sigterm),
      ])
      self.assertEqual(self.getsignal_mock.call_count, 2)

    # Verify various args are passed down to the real command.
    pargs = self.popen_mock.call_args[0][0]
    self.assertEqual(cmd, pargs)

    # Verify various kwargs are passed down to the real command.
    pkwargs = self.popen_mock.call_args[1]
    for key in ('cwd', 'stdin', 'stdout', 'stderr'):
      kwargs.setdefault(key, None)
    kwargs.setdefault('shell', False)
    kwargs.setdefault('env', mock.ANY)
    kwargs['close_fds'] = True
    self.longMessage = True
    for key in kwargs.keys():
      self.assertEqual(kwargs[key], pkwargs[key],
                       msg='kwargs[%s] mismatch' % key)

  def _AssertCrEqual(self, expected, actual):
    """Helper method to compare two CommandResult objects.

    This is needed since assertEqual does not know how to compare two
    CommandResult objects.

    Args:
      expected: a CommandResult object, expected result.
      actual: a CommandResult object, actual result.
    """
    self.assertEqual(expected.args, actual.args)
    self.assertEqual(expected.stderr, actual.stderr)
    self.assertEqual(expected.stdout, actual.stdout)
    self.assertEqual(expected.returncode, actual.returncode)

  @_ForceLoggingLevel
  def _TestCmd(self, cmd, real_cmd, sp_kv=None, rc_kv=None, sudo=False):
    """Factor out common setup logic for testing run().

    Args:
      cmd: a string or an array of strings that will be passed to run.
      real_cmd: the real command we expect run to call (might be
          modified to have enter_chroot).
      sp_kv: key-value pairs passed to subprocess.Popen().
      rc_kv: key-value pairs passed to run().
      sudo: use sudo_run() rather than run().
    """
    if sp_kv is None:
      sp_kv = {}
    if rc_kv is None:
      rc_kv = {}

    expected_result = cros_build_lib.CommandResult(
        args=real_cmd, stdout=self.output, stderr=self.error,
        returncode=self.proc_mock.returncode)

    arg_dict = dict()
    for attr in ('close_fds', 'cwd', 'env', 'stdin', 'stdout', 'stderr',
                 'shell'):
      if attr in sp_kv:
        arg_dict[attr] = sp_kv[attr]
      else:
        if attr == 'close_fds':
          arg_dict[attr] = True
        elif attr == 'shell':
          arg_dict[attr] = False
        else:
          arg_dict[attr] = None

    if sudo:
      runcmd = cros_build_lib.sudo_run
    else:
      runcmd = cros_build_lib.run
    with self._MockChecker(real_cmd, ignore_sigint=rc_kv.get('ignore_sigint'),
                           **sp_kv):
      actual_result = runcmd(cmd, **rc_kv)

    # If run was called with encoding, we need to encode the result
    # before making a comparison below, as the underlying data we are
    # passing to _Popen is stored as bytes.
    if 'encoding' in rc_kv:
      encoding = rc_kv['encoding']
      actual_result.stdout = actual_result.stdout.encode(encoding)
      actual_result.stderr = actual_result.stderr.encode(encoding)

    self._AssertCrEqual(expected_result, actual_result)

  def testReturnCodeZeroWithArrayCmd(self, ignore_sigint=False):
    """--enter_chroot=False and --cmd is an array of strings.

    Parameterized so this can also be used by some other tests w/ alternate
    params to run().

    Args:
      ignore_sigint: If True, we'll tell run to ignore sigint.
    """
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']
    self._TestCmd(cmd_list, cmd_list,
                  rc_kv=dict(ignore_sigint=ignore_sigint))

  def testSignalRestoreNormalCase(self):
    """Test run() properly sets/restores sigint.  Normal case."""
    self.testReturnCodeZeroWithArrayCmd(ignore_sigint=True)

  def testReturnCodeZeroWithArrayCmdEnterChroot(self):
    """--enter_chroot=True and --cmd is an array of strings."""
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']
    real_cmd = cmd_list
    if not cros_build_lib.IsInsideChroot():
      real_cmd = ['cros_sdk', '--'] + cmd_list
    self._TestCmd(cmd_list, real_cmd, rc_kv=dict(enter_chroot=True))

  @_ForceLoggingLevel
  def testCommandFailureRaisesError(self, ignore_sigint=False):
    """Verify error raised by communicate() is caught.

    Parameterized so this can also be used by some other tests w/ alternate
    params to run().

    Args:
      ignore_sigint: If True, we'll tell run to ignore sigint.
    """
    cmd = 'test cmd'
    self.proc_mock.returncode = 1
    with self._MockChecker(['/bin/bash', '-c', cmd],
                           ignore_sigint=ignore_sigint):
      self.assertRaises(cros_build_lib.RunCommandError,
                        cros_build_lib.run, cmd, shell=True,
                        ignore_sigint=ignore_sigint, error_code_ok=False)

  @_ForceLoggingLevel
  def testSubprocessCommunicateExceptionRaisesError(self, ignore_sigint=False):
    """Verify error raised by communicate() is caught.

    Parameterized so this can also be used by some other tests w/ alternate
    params to run().

    Args:
      ignore_sigint: If True, we'll tell run to ignore sigint.
    """
    cmd = ['test', 'cmd']
    self.proc_mock.communicate = mock.MagicMock(side_effect=ValueError)
    with self._MockChecker(cmd, ignore_sigint=ignore_sigint):
      self.assertRaises(ValueError, cros_build_lib.run, cmd,
                        ignore_sigint=ignore_sigint)

  def testSignalRestoreExceptionCase(self):
    """Test run() properly sets/restores sigint.  Exception case."""
    self.testSubprocessCommunicateExceptionRaisesError(ignore_sigint=True)

  def testEnvWorks(self):
    """Test run(..., env=xyz) works."""
    # We'll put this bogus environment together, just to make sure
    # subprocess.Popen gets passed it.
    rc_env = {'Tom': 'Jerry', 'Itchy': 'Scratchy'}
    sp_env = dict(rc_env, LC_MESSAGES='C')

    # This is a simple case, copied from testReturnCodeZeroWithArrayCmd()
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']

    # Run.  We expect the env= to be passed through from sp (subprocess.Popen)
    # to rc (run).
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv=dict(env=sp_env),
                  rc_kv=dict(env=rc_env))

  def testExtraEnvOnlyWorks(self):
    """Test run(..., extra_env=xyz) works."""
    # We'll put this bogus environment together, just to make sure
    # subprocess.Popen gets passed it.
    extra_env = {'Pinky' : 'Brain'}
    ## This is a little bit circular, since the same logic is used to compute
    ## the value inside, but at least it checks that this happens.
    total_env = os.environ.copy()
    # The core run code forces this too.
    total_env['LC_MESSAGES'] = 'C'
    total_env.update(extra_env)

    # This is a simple case, copied from testReturnCodeZeroWithArrayCmd()
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']

    # Run.  We expect the env= to be passed through from sp (subprocess.Popen)
    # to rc (run).
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv=dict(env=total_env),
                  rc_kv=dict(extra_env=extra_env))

  def testExtraEnvTooWorks(self):
    """Test run(..., env=xy, extra_env=z) works."""
    # We'll put this bogus environment together, just to make sure
    # subprocess.Popen gets passed it.
    env = {'Tom': 'Jerry', 'Itchy': 'Scratchy'}
    extra_env = {'Pinky': 'Brain'}
    total_env = {
        'Tom': 'Jerry',
        'Itchy': 'Scratchy',
        'Pinky': 'Brain',
        'LC_MESSAGES': 'C'
    }

    # This is a simple case, copied from testReturnCodeZeroWithArrayCmd()
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']

    # Run.  We expect the env= to be passed through from sp (subprocess.Popen)
    # to rc (run).
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv=dict(env=total_env),
                  rc_kv=dict(env=env, extra_env=extra_env))

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testChrootExtraEnvWorks(self, _inchroot_mock):
    """Test run(..., enter_chroot=True, env=xy, extra_env=z) works."""
    # We'll put this bogus environment together, just to make sure
    # subprocess.Popen gets passed it.
    env = {'Tom': 'Jerry', 'Itchy': 'Scratchy'}
    extra_env = {'Pinky': 'Brain'}
    total_env = {
        'Tom': 'Jerry',
        'Itchy': 'Scratchy',
        'Pinky': 'Brain',
        'LC_MESSAGES': 'C'
    }

    # This is a simple case, copied from testReturnCodeZeroWithArrayCmd()
    self.proc_mock.returncode = 0
    cmd_list = ['foo', 'bar', 'roger']

    # Run.  We expect the env= to be passed through from sp (subprocess.Popen)
    # to rc (run).
    self._TestCmd(cmd_list, ['cros_sdk', 'Pinky=Brain', '--'] + cmd_list,
                  sp_kv=dict(env=total_env),
                  rc_kv=dict(env=env, extra_env=extra_env, enter_chroot=True))

  def testExceptionEquality(self):
    """Verify equality methods for RunCommandError"""

    c1 = cros_build_lib.CommandResult(cmd=['ls', 'arg'], returncode=1)
    c2 = cros_build_lib.CommandResult(cmd=['ls', 'arg1'], returncode=1)
    c3 = cros_build_lib.CommandResult(cmd=['ls', 'arg'], returncode=2)
    e1 = cros_build_lib.RunCommandError('Message 1', c1)
    e2 = cros_build_lib.RunCommandError('Message 1', c1)
    e_diff_msg = cros_build_lib.RunCommandError('Message 2', c1)
    e_diff_cmd = cros_build_lib.RunCommandError('Message 1', c2)
    e_diff_code = cros_build_lib.RunCommandError('Message 1', c3)

    self.assertEqual(e1, e2)
    self.assertNotEqual(e1, e_diff_msg)
    self.assertNotEqual(e1, e_diff_cmd)
    self.assertNotEqual(e1, e_diff_code)

  def testSudoRunCommand(self):
    """Test sudo_run(...) works."""
    cmd_list = ['foo', 'bar', 'roger']
    sudo_list = ['sudo', '--'] + cmd_list
    self.proc_mock.returncode = 0
    self._TestCmd(cmd_list, sudo_list, sudo=True)

  def testSudoRunCommandShell(self):
    """Test sudo_run(..., shell=True) works."""
    cmd = 'foo bar roger'
    sudo_list = ['sudo', '--', '/bin/bash', '-c', cmd]
    self.proc_mock.returncode = 0
    self._TestCmd(cmd, sudo_list, sudo=True,
                  rc_kv=dict(shell=True))

  def testSudoRunCommandEnv(self):
    """Test sudo_run(..., extra_env=z) works."""
    cmd_list = ['foo', 'bar', 'roger']
    sudo_list = ['sudo', 'shucky=ducky', '--'] + cmd_list
    extra_env = {'shucky' : 'ducky'}
    self.proc_mock.returncode = 0
    self._TestCmd(cmd_list, sudo_list, sudo=True,
                  rc_kv=dict(extra_env=extra_env))

  def testSudoRunCommandUser(self):
    """Test sudo_run(..., user='...') works."""
    cmd_list = ['foo', 'bar', 'roger']
    sudo_list = ['sudo', '-u', 'MMMMMonster', '--'] + cmd_list
    self.proc_mock.returncode = 0
    self._TestCmd(cmd_list, sudo_list, sudo=True,
                  rc_kv=dict(user='MMMMMonster'))

  def testSudoRunCommandUserShell(self):
    """Test sudo_run(..., user='...', shell=True) works."""
    cmd = 'foo bar roger'
    sudo_list = ['sudo', '-u', 'MMMMMonster', '--', '/bin/bash', '-c', cmd]
    self.proc_mock.returncode = 0
    self._TestCmd(cmd, sudo_list, sudo=True,
                  rc_kv=dict(user='MMMMMonster', shell=True))

  def testInputBytes(self):
    """Test that we can always pass non-UTF-8 bytes as input."""
    cmd_list = ['foo', 'bar', 'roger']
    bytes_input = b'\xff'
    self.proc_mock.returncode = 0
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv={'stdin': subprocess.PIPE},
                  rc_kv={'input': bytes_input, 'encoding': 'utf-8'})
    self.assertEqual(self.stdin, bytes_input)

  def testInputString(self):
    """Test that we encode UTF-8 strings passed on input."""
    cmd_list = ['foo', 'bar', 'roger']
    unicode_input = u'💩'
    self.proc_mock.returncode = 0
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv={'stdin': subprocess.PIPE},
                  rc_kv={'input': unicode_input, 'encoding': 'utf-8'})
    self.assertEqual(self.stdin, unicode_input.encode('utf-8'))

  def testInputStringNoEncoding(self):
    """Test that we encode UTF-8 strings passed on input without
    passing encoding parameter."""
    cmd_list = ['foo', 'bar', 'roger']
    unicode_input = u'💩'
    self.proc_mock.returncode = 0
    self._TestCmd(cmd_list, cmd_list,
                  sp_kv={'stdin': subprocess.PIPE},
                  rc_kv={'input': unicode_input})
    self.assertEqual(self.stdin, unicode_input.encode('utf-8'))


class TestRunCommandOutput(cros_test_lib.TempDirTestCase,
                           cros_test_lib.OutputTestCase):
  """Tests of run output options."""

  @_ForceLoggingLevel
  def testLogStdoutToFile(self):
    log = os.path.join(self.tempdir, 'output')
    ret = cros_build_lib.run(['echo', 'monkeys'], log_stdout_to_file=log)
    self.assertEqual(osutils.ReadFile(log), 'monkeys\n')
    self.assertIs(ret.output, None)
    self.assertIs(ret.error, None)

    os.unlink(log)
    ret = cros_build_lib.run(
        ['sh', '-c', 'echo monkeys3 >&2'],
        log_stdout_to_file=log, redirect_stderr=True)
    self.assertEqual(ret.error, b'monkeys3\n')
    self.assertExists(log)
    self.assertEqual(os.path.getsize(log), 0)

    os.unlink(log)
    ret = cros_build_lib.run(
        ['sh', '-c', 'echo monkeys4; echo monkeys5 >&2'],
        log_stdout_to_file=log, combine_stdout_stderr=True)
    self.assertIs(ret.output, None)
    self.assertIs(ret.error, None)
    self.assertEqual(osutils.ReadFile(log), 'monkeys4\nmonkeys5\n')


  @_ForceLoggingLevel
  def testLogStdoutToFileWithOrWithoutAppend(self):
    log = os.path.join(self.tempdir, 'output')
    ret = cros_build_lib.run(['echo', 'monkeys'], log_stdout_to_file=log)
    self.assertEqual(osutils.ReadFile(log), 'monkeys\n')
    self.assertIs(ret.output, None)
    self.assertIs(ret.error, None)

    # Without append
    ret = cros_build_lib.run(['echo', 'monkeys2'], log_stdout_to_file=log)
    self.assertEqual(osutils.ReadFile(log), 'monkeys2\n')
    self.assertIs(ret.output, None)
    self.assertIs(ret.error, None)

    # With append
    ret = cros_build_lib.run(
        ['echo', 'monkeys3'], append_to_file=True, log_stdout_to_file=log)
    self.assertEqual(osutils.ReadFile(log), 'monkeys2\nmonkeys3\n')
    self.assertIs(ret.output, None)
    self.assertIs(ret.error, None)


  def _CaptureRunCommand(self, command, mute_output):
    """Capture a run() output with the specified |mute_output|.

    Args:
      command: command to send to run().
      mute_output: run() |mute_output| parameter.

    Returns:
      A (stdout, stderr) pair of captured output.
    """
    with self.OutputCapturer() as output:
      cros_build_lib.run(command, debug_level=logging.DEBUG,
                         mute_output=mute_output)
    return (output.GetStdout(), output.GetStderr())

  @_ForceLoggingLevel
  def testSubprocessMuteOutput(self):
    """Test run |mute_output| parameter."""
    command = ['sh', '-c', 'echo foo; echo bar >&2']
    # Always mute: we shouldn't get any output.
    self.assertEqual(self._CaptureRunCommand(command, mute_output=True),
                     ('', ''))
    # Mute based on |debug_level|: we should't get any output.
    self.assertEqual(self._CaptureRunCommand(command, mute_output=None),
                     ('', ''))
    # Never mute: we should get 'foo\n' and 'bar\n'.
    self.assertEqual(self._CaptureRunCommand(command, mute_output=False),
                     ('foo\n', 'bar\n'))

  def testRunCommandAtNoticeLevel(self):
    """Ensure that run prints output when mute_output is False."""
    # Needed by cros_sdk and brillo/cros chroot.
    with self.OutputCapturer():
      cros_build_lib.run(['echo', 'foo'], mute_output=False,
                         error_code_ok=True, print_cmd=False,
                         debug_level=logging.NOTICE)
    self.AssertOutputContainsLine('foo')

  def testRunCommandRedirectStdoutStderrOnCommandError(self):
    """Tests that stderr is captured when run raises."""
    with self.assertRaises(cros_build_lib.RunCommandError) as cm:
      cros_build_lib.run(['cat', '/'], redirect_stderr=True)
    self.assertIsNotNone(cm.exception.result.error)
    self.assertNotEqual('', cm.exception.result.error)

  def _CaptureLogOutput(self, cmd, **kwargs):
    """Capture logging output of run."""
    log = os.path.join(self.tempdir, 'output')
    fh = logging.FileHandler(log)
    fh.setLevel(logging.DEBUG)
    logging.getLogger().addHandler(fh)
    cros_build_lib.run(cmd, **kwargs)
    logging.getLogger().removeHandler(fh)
    output = osutils.ReadFile(log)
    fh.close()
    return output

  @_ForceLoggingLevel
  def testLogOutput(self):
    """Normal log_output, stdout followed by stderr."""
    cmd = 'echo Greece; echo Italy >&2; echo Spain'
    log_output = ('run: /bin/bash -c '
                  "'echo Greece; echo Italy >&2; echo Spain'\n"
                  '(stdout):\nGreece\nSpain\n\n(stderr):\nItaly\n\n')
    output = self._CaptureLogOutput(cmd, shell=True, log_output=True,
                                    encoding='utf-8')
    self.assertEqual(output, log_output)


class TestTimedSection(cros_test_lib.TestCase):
  """Tests for TimedSection context manager."""

  def testTimerValues(self):
    """Make sure simple stuff works."""
    with cros_build_lib.TimedSection() as timer:
      # While running, we have access to the start time.
      self.assertIsInstance(timer.start, datetime.datetime)
      self.assertIsNone(timer.finish)
      self.assertIsNone(timer.delta)

    # After finishing, all values should be set.
    self.assertIsInstance(timer.start, datetime.datetime)
    self.assertIsInstance(timer.finish, datetime.datetime)
    self.assertIsInstance(timer.delta, datetime.timedelta)


class HelperMethodSimpleTests(cros_test_lib.OutputTestCase):
  """Tests for various helper methods without using mocks."""

  def testUserDateTime(self):
    """Test with a raw time value."""
    expected = 'Mon, 16 Jun 1980 05:03:20 -0700 (PDT)'
    with cros_test_lib.SetTimeZone('US/Pacific'):
      timeval = 330005000
      self.assertEqual(cros_build_lib.UserDateTimeFormat(timeval=timeval),
                       expected)

  def testUserDateTimeDateTime(self):
    """Test with a datetime object."""
    expected = 'Mon, 16 Jun 1980 00:00:00 -0700 (PDT)'
    with cros_test_lib.SetTimeZone('US/Pacific'):
      timeval = datetime.datetime(1980, 6, 16)
      self.assertEqual(cros_build_lib.UserDateTimeFormat(timeval=timeval),
                       expected)

  def testUserDateTimeDateTimeInWinter(self):
    """Test that we correctly switch from PDT to PST."""
    expected = 'Wed, 16 Jan 1980 00:00:00 -0800 (PST)'
    with cros_test_lib.SetTimeZone('US/Pacific'):
      timeval = datetime.datetime(1980, 1, 16)
      self.assertEqual(cros_build_lib.UserDateTimeFormat(timeval=timeval),
                       expected)

  def testUserDateTimeDateTimeInEST(self):
    """Test that we correctly switch from PDT to EST."""
    expected = 'Wed, 16 Jan 1980 00:00:00 -0500 (EST)'
    with cros_test_lib.SetTimeZone('US/Eastern'):
      timeval = datetime.datetime(1980, 1, 16)
      self.assertEqual(cros_build_lib.UserDateTimeFormat(timeval=timeval),
                       expected)

  def testUserDateTimeCurrentTime(self):
    """Test that we can get the current time."""
    cros_build_lib.UserDateTimeFormat()

  def testParseUserDateTimeFormat(self):
    stringtime = cros_build_lib.UserDateTimeFormat(100000.0)
    self.assertEqual(cros_build_lib.ParseUserDateTimeFormat(stringtime),
                     100000.0)

  def testGetRandomString(self):
    """Verify it looks sane."""
    data = cros_build_lib.GetRandomString()
    self.assertRegex(data, r'^[a-z0-9]+$')
    self.assertEqual(32, len(data))

  def testMachineDetails(self):
    """Verify we don't crash."""
    contents = cros_build_lib.MachineDetails()
    self.assertNotEqual(contents, '')
    self.assertEqual(contents[-1], '\n')


class TestInput(cros_test_lib.MockOutputTestCase):
  """Tests of input gathering functionality."""

  def testGetInput(self):
    """Verify GetInput() basic behavior."""
    response = 'Some response'
    if sys.version_info.major < 3:
      self.PatchObject(builtins, 'raw_input', return_value=response)
    self.PatchObject(builtins, 'input', return_value=response)
    self.assertEqual(response, cros_build_lib.GetInput('prompt'))

  def testBooleanPrompt(self):
    """Verify BooleanPrompt() full behavior."""
    m = self.PatchObject(cros_build_lib, 'GetInput')

    m.return_value = ''
    self.assertTrue(cros_build_lib.BooleanPrompt())
    self.assertFalse(cros_build_lib.BooleanPrompt(default=False))

    m.return_value = 'yes'
    self.assertTrue(cros_build_lib.BooleanPrompt())
    m.return_value = 'ye'
    self.assertTrue(cros_build_lib.BooleanPrompt())
    m.return_value = 'y'
    self.assertTrue(cros_build_lib.BooleanPrompt())

    m.return_value = 'no'
    self.assertFalse(cros_build_lib.BooleanPrompt())
    m.return_value = 'n'
    self.assertFalse(cros_build_lib.BooleanPrompt())

  def testBooleanShellValue(self):
    """Verify BooleanShellValue() inputs work as expected"""
    for v in (None,):
      self.assertTrue(cros_build_lib.BooleanShellValue(v, True))
      self.assertFalse(cros_build_lib.BooleanShellValue(v, False))

    for v in (1234, '', 'akldjsf', '"'):
      self.assertRaises(ValueError, cros_build_lib.BooleanShellValue, v, True)
      self.assertTrue(cros_build_lib.BooleanShellValue(v, True, msg=''))
      self.assertFalse(cros_build_lib.BooleanShellValue(v, False, msg=''))

    for v in ('yes', 'YES', 'YeS', 'y', 'Y', '1', 'true', 'True', 'TRUE',):
      self.assertTrue(cros_build_lib.BooleanShellValue(v, True))
      self.assertTrue(cros_build_lib.BooleanShellValue(v, False))

    for v in ('no', 'NO', 'nO', 'n', 'N', '0', 'false', 'False', 'FALSE',):
      self.assertFalse(cros_build_lib.BooleanShellValue(v, True))
      self.assertFalse(cros_build_lib.BooleanShellValue(v, False))

  def testGetChoiceLists(self):
    """Verify GetChoice behavior w/lists."""
    m = self.PatchObject(cros_build_lib, 'GetInput')

    m.return_value = '1'
    ret = cros_build_lib.GetChoice('title', ['a', 'b', 'c'])
    self.assertEqual(ret, 1)

  def testGetChoiceGenerator(self):
    """Verify GetChoice behavior w/generators."""
    m = self.PatchObject(cros_build_lib, 'GetInput')

    m.return_value = '2'
    ret = cros_build_lib.GetChoice('title', list(range(3)))
    self.assertEqual(ret, 2)

  def testGetChoiceWindow(self):
    """Verify GetChoice behavior w/group_size set."""
    m = self.PatchObject(cros_build_lib, 'GetInput')

    cnt = [0]
    def _Gen():
      while True:
        cnt[0] += 1
        yield 'a'

    m.side_effect = ['\n', '2']
    ret = cros_build_lib.GetChoice('title', _Gen(), group_size=2)
    self.assertEqual(ret, 2)

    # Verify we showed the correct number of times.
    self.assertEqual(cnt[0], 5)


class TestContextManagerStack(cros_test_lib.TestCase):
  """Test the ContextManagerStack class."""

  def test(self):
    invoked = []
    counter = functools.partial(next, itertools.count())
    def _mk_kls(has_exception=None, exception_kls=None, suppress=False):
      class foon(object):
        """Simple context manager which runs checks on __exit__."""
        marker = counter()
        def __enter__(self):
          return self

        # pylint: disable=no-self-argument,bad-context-manager
        def __exit__(obj_self, exc_type, exc, traceback):
          invoked.append(obj_self.marker)
          if has_exception is not None:
            self.assertTrue(all(x is not None
                                for x in (exc_type, exc, traceback)))
            self.assertTrue(exc_type == has_exception)
          if exception_kls:
            raise exception_kls()
          if suppress:
            return True
      return foon

    with cros_build_lib.ContextManagerStack() as stack:
      # Note... these tests are in reverse, since the exception
      # winds its way up the stack.
      stack.Add(_mk_kls())
      stack.Add(_mk_kls(ValueError, suppress=True))
      stack.Add(_mk_kls(IndexError, exception_kls=ValueError))
      stack.Add(_mk_kls(IndexError))
      stack.Add(_mk_kls(exception_kls=IndexError))
      stack.Add(_mk_kls())
    self.assertEqual(invoked, list(range(5, -1, -1)))


class Test_iflatten_instance(cros_test_lib.TestCase):
  """Test iflatten_instance function."""

  def test_it(self):
    f = lambda x, **kwargs: list(cros_build_lib.iflatten_instance(x, **kwargs))
    self.assertEqual([1, 2], f([1, 2]))
    self.assertEqual([1, '2a'], f([1, '2a']))
    self.assertEqual([1, 2, 'b'], f([1, [2, 'b']]))
    self.assertEqual([1, 2, 'f', 'd', 'a', 's'],
                     f([1, 2, ('fdas',)], terminate_on_kls=int))
    self.assertEqual([''], f(''))
    self.assertEqual([b''], f(b''))
    self.assertEqual([b'1234'], f(b'1234'))
    self.assertEqual([b'12', b'34'], f([b'12', b'34']))


class SafeRunTest(cros_test_lib.TestCase):
  """Tests SafeRunTest functionality."""

  def _raise_exception(self, e):
    raise e

  def testRunsSafely(self):
    """Verify that we are robust to exceptions."""
    def append_val(value):
      call_list.append(value)

    call_list = []
    f_list = [functools.partial(append_val, 1),
              functools.partial(self._raise_exception,
                                Exception('testRunsSafely exception.')),
              functools.partial(append_val, 2)]
    self.assertRaises(Exception, cros_build_lib.SafeRun, f_list)
    self.assertEqual(call_list, [1, 2])

  def testRaisesFirstException(self):
    """Verify we raise the first exception when multiple are encountered."""
    class E1(Exception):
      """Simple exception class."""
      pass

    class E2(Exception):
      """Simple exception class."""
      pass

    f_list = [functools.partial(self._raise_exception, e) for e in [E1, E2]]
    self.assertRaises(E1, cros_build_lib.SafeRun, f_list)

  def testCombinedRaise(self):
    """Raises a RuntimeError with exceptions combined."""
    f_list = [functools.partial(self._raise_exception, Exception())] * 3
    self.assertRaises(RuntimeError, cros_build_lib.SafeRun, f_list,
                      combine_exceptions=True)


class TestGetHostname(cros_test_lib.MockTestCase):
  """Tests GetHostName & GetHostDomain functionality."""

  def setUp(self):
    self.gethostname_mock = self.PatchObject(
        socket, 'gethostname', return_value='m!!n')
    self.gethostbyaddr_mock = self.PatchObject(
        socket, 'gethostbyaddr', return_value=(
            'm!!n.google.com', ('cow', 'bar',), ('127.0.0.1.a',)))

  def testGetHostNameNonQualified(self):
    """Verify non-qualified behavior"""
    self.assertEqual(cros_build_lib.GetHostName(), 'm!!n')

  def testGetHostNameFullyQualified(self):
    """Verify fully qualified behavior"""
    self.assertEqual(cros_build_lib.GetHostName(fully_qualified=True),
                     'm!!n.google.com')

  def testGetHostNameBadDns(self):
    """Do not fail when the user's dns is bad"""
    self.gethostbyaddr_mock.side_effect = socket.gaierror('should be caught')
    self.assertEqual(cros_build_lib.GetHostName(), 'm!!n')

  def testGetHostDomain(self):
    """Verify basic behavior"""
    self.assertEqual(cros_build_lib.GetHostDomain(), 'google.com')

  def testHostIsCIBuilder(self):
    """Test HostIsCIBuilder."""
    fq_hostname_golo = 'test.golo.chromium.org'
    fq_hostname_gce_1 = 'test.chromeos-bot.internal'
    fq_hostname_gce_2 = 'test.chrome.corp.google.com'
    fq_hostname_invalid = 'test'
    self.assertTrue(cros_build_lib.HostIsCIBuilder(fq_hostname_golo))
    self.assertTrue(cros_build_lib.HostIsCIBuilder(fq_hostname_gce_1))
    self.assertTrue(cros_build_lib.HostIsCIBuilder(fq_hostname_gce_2))
    self.assertFalse(cros_build_lib.HostIsCIBuilder(fq_hostname_invalid))
    self.assertFalse(cros_build_lib.HostIsCIBuilder(
        fq_hostname=fq_hostname_golo, gce_only=True))
    self.assertFalse(cros_build_lib.HostIsCIBuilder(
        fq_hostname=fq_hostname_gce_1, golo_only=True))


class CreateTarballTests(cros_test_lib.TempDirTestCase):
  """Test the CreateTarball function."""

  def setUp(self):
    """Create files/dirs needed for tar test."""
    self.target = os.path.join(self.tempdir, 'test.tar.xz')
    self.inputDir = os.path.join(self.tempdir, 'inputs')
    self.inputs = [
        'inputA',
        'inputB',
        'sub/subfile',
        'sub2/subfile',
    ]

    self.inputsWithDirs = [
        'inputA',
        'inputB',
        'sub',
        'sub2',
    ]


    # Create the input files.
    for i in self.inputs:
      osutils.WriteFile(os.path.join(self.inputDir, i), i, makedirs=True)

  def testSuccess(self):
    """Create a tarfile."""
    cros_build_lib.CreateTarball(self.target, self.inputDir,
                                 inputs=self.inputs)

  def testSuccessWithDirs(self):
    """Create a tarfile."""
    cros_build_lib.CreateTarball(self.target, self.inputDir,
                                 inputs=self.inputsWithDirs)

  def testSuccessWithTooManyFiles(self):
    """Test a tarfile creation with -T /dev/stdin."""
    # pylint: disable=protected-access
    num_inputs = cros_build_lib._THRESHOLD_TO_USE_T_FOR_TAR + 1
    inputs = ['input%s' % x for x in range(num_inputs)]
    largeInputDir = os.path.join(self.tempdir, 'largeinputs')
    for i in inputs:
      osutils.WriteFile(os.path.join(largeInputDir, i), i, makedirs=True)
    cros_build_lib.CreateTarball(self.target, largeInputDir, inputs=inputs)


# Tests for tar failure retry logic.

class FailedCreateTarballTests(cros_test_lib.MockTestCase):
  """Tests special case error handling for CreateTarBall."""

  def setUp(self):
    """Mock run mock."""
    # Each test can change this value as needed.  Each element is the return
    # code in the CommandResult for subsequent calls to run().
    self.tarResults = []

    def Result(*_args, **_kwargs):
      """Creates CommandResult objects for each tarResults value in turn."""
      return cros_build_lib.CommandResult(returncode=self.tarResults.pop(0))

    self.mockRun = self.PatchObject(cros_build_lib, 'run',
                                    autospec=True,
                                    side_effect=Result)

  def testSuccess(self):
    """CreateTarball works the first time."""
    self.tarResults = [0]
    cros_build_lib.CreateTarball('foo', 'bar', inputs=['a', 'b'])

    self.assertEqual(self.mockRun.call_count, 1)

  def testFailedOnceSoft(self):
    """Force a single retry for CreateTarball."""
    self.tarResults = [1, 0]
    cros_build_lib.CreateTarball('foo', 'bar', inputs=['a', 'b'], timeout=0)

    self.assertEqual(self.mockRun.call_count, 2)

  def testFailedOnceHard(self):
    """Test unrecoverable error."""
    self.tarResults = [2]
    with self.assertRaises(cros_build_lib.RunCommandError) as cm:
      cros_build_lib.CreateTarball('foo', 'bar', inputs=['a', 'b'])

    self.assertEqual(self.mockRun.call_count, 1)
    self.assertEqual(cm.exception.args[1].returncode, 2)

  def testFailedThriceSoft(self):
    """Exhaust retries for recoverable errors."""
    self.tarResults = [1, 1, 1]
    with self.assertRaises(cros_build_lib.RunCommandError) as cm:
      cros_build_lib.CreateTarball('foo', 'bar', inputs=['a', 'b'], timeout=0)

    self.assertEqual(self.mockRun.call_count, 3)
    self.assertEqual(cm.exception.args[1].returncode, 1)
