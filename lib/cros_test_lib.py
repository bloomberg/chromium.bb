#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cros unit test library, with utility functions."""

from __future__ import print_function
import collections
import cStringIO
import errno
import exceptions
import functools
import httplib
import json
import logging
import mox
import netrc
import os
import re
import signal
import socket
import stat
import sys
import unittest

from chromite.buildbot import constants
import cros_build_lib
import git
import gob_util
import osutils
import terminal

if 'chromite' not in sys.modules:
  # TODO(build): Finish test wrapper (http://crosbug.com/37517).
  # Until then, we detect the chromite manipulation not yet having
  # occurred, and inject it ourselves.
  # We cannot just import chromite since this module is still accessed
  # from non chromite.lib.cros_test_lib pathways (which will be resolved
  # implicitly via 37517).
  sys.path.insert(0, os.path.join(
      os.path.dirname(os.path.abspath(__file__)), '../third_party'))

import mock


Directory = collections.namedtuple('Directory', ['name', 'contents'])


class GlobalTestConfig(object):
  """Global configuration for tests."""

  # By default, disable all network tests.
  NETWORK_TESTS_DISABLED = True


def NetworkTest(reason='Skipping network test'):
  """Decorator for unit tests. Skip the test if --network is not specified."""
  def Decorator(test_item):
    @functools.wraps(test_item)
    def NetworkWrapper(*args, **kwargs):
      if GlobalTestConfig.NETWORK_TESTS_DISABLED:
        raise unittest.SkipTest(reason)
      test_item(*args, **kwargs)

    if not (isinstance(test_item, type) and issubclass(test_item, TestCase)):
      return NetworkWrapper
    return test_item

  return Decorator


def _FlattenStructure(base_path, dir_struct):
  """Converts a directory structure to a list of paths."""
  flattened = []
  for obj in dir_struct:
    if isinstance(obj, Directory):
      new_base = os.path.join(base_path, obj.name).rstrip(os.sep)
      flattened.append(new_base + os.sep)
      flattened.extend(_FlattenStructure(new_base, obj.contents))
    else:
      assert(isinstance(obj, basestring))
      flattened.append(os.path.join(base_path, obj))
  return flattened


def CreateOnDiskHierarchy(base_path, dir_struct):
  """Creates on-disk representation of an in-memory directory structure.

  Arguments:
    base_path: The absolute root of the directory structure.
    dir_struct: A recursively defined data structure that represents a
      directory tree.  The basic form is a list.  Elements can be file names or
      cros_test_lib.Directory objects.  The 'contents' attribute of Directory
      types is a directory structure representing the contents of the directory.
      Examples:
        - ['file1', 'file2']
        - ['file1', Directory('directory', ['deepfile1', 'deepfile2']), 'file2']
  """
  flattened = _FlattenStructure(base_path, dir_struct)
  for f in flattened:
    f = os.path.join(base_path, f)
    if f.endswith(os.sep):
      osutils.SafeMakedirs(f)
    else:
      osutils.Touch(f, makedirs=True)


def _VerifyDirectoryIterables(existing, expected):
  """Compare two iterables representing contents of a directory.

  Paths in |existing| and |expected| will be compared for exact match.

  Arguments:
    existing: An iterable containing paths that exist.
    expected: An iterable of paths that are expected.

  Raises:
    AssertionError when there is any divergence between |existing| and
    |expected|.
  """
  def FormatPaths(paths):
    return '\n'.join(sorted(paths))

  existing = set(existing)
  expected = set(expected)

  unexpected = existing - expected
  if unexpected:
    raise AssertionError('Found unexpected paths:\n%s'
                         % FormatPaths(unexpected))
  missing = expected - existing
  if missing:
    raise AssertionError('These files were expected but not found:\n%s'
                         % FormatPaths(missing))


def VerifyOnDiskHierarchy(base_path, dir_struct):
  """Verify that an on-disk directory tree exactly matches a given structure.

  Arguments:
    See arguments of CreateOnDiskHierarchy()

  Raises:
    AssertionError when there is any divergence between the on-disk
    structure and the structure specified by 'dir_struct'.
  """
  expected = _FlattenStructure(base_path, dir_struct)
  _VerifyDirectoryIterables(osutils.DirectoryIterator(base_path), expected)


def VerifyTarball(tarball, dir_struct):
  """Compare the contents of a tarball against a directory structure.

  Arguments:
    tarball: Path to the tarball.
    dir_struct: See CreateOnDiskHierarchy()

  Raises:
    AssertionError when there is any divergence between the tarball and the
    structure specified by 'dir_struct'.
  """
  contents = cros_build_lib.RunCommandCaptureOutput(
      ['tar', '-tf', tarball]).output.splitlines()
  normalized = set()
  for p in contents:
    norm = os.path.normpath(p)
    if p.endswith('/'):
      norm += '/'
    if norm in normalized:
      raise AssertionError('Duplicate entry %r found in %r!' % (norm, tarball))
    normalized.add(norm)

  expected = _FlattenStructure('', dir_struct)
  _VerifyDirectoryIterables(normalized, expected)


def _walk_mro_stacking(obj, attr, reverse=False):
  iterator = iter if reverse else reversed
  methods = (getattr(x, attr, None) for x in iterator(obj.__class__.__mro__))
  seen = set()
  for x in filter(None, methods):
    x = getattr(x, 'im_func', x)
    if x not in seen:
      seen.add(x)
      yield x


def _stacked_setUp(self):
  self.__test_was_run__ = False
  try:
    for target in _walk_mro_stacking(self, '__raw_setUp__'):
      target(self)
  except:
    # TestCase doesn't trigger tearDowns if setUp failed; thus
    # manually force it ourselves to ensure cleanup occurs.
    _stacked_tearDown(self)
    raise

  # Now mark the object as fully setUp; this is done so that
  # any last minute assertions in tearDown can know if they should
  # run or not.
  self.__test_was_run__ = True


def _stacked_tearDown(self):
  exc_info = None
  for target in _walk_mro_stacking(self, '__raw_tearDown__', True):
    #pylint: disable=W0702
    try:
      target(self)
    except:
      # Preserve the exception, throw it after running
      # all tearDowns; we throw just the first also.  We suppress
      # pylint's warning here since it can't understand that we're
      # actually raising the exception, just in a nonstandard way.
      if exc_info is None:
        exc_info = sys.exc_info()

  if exc_info:
    # Chuck the saved exception, w/ the same TB from
    # when it occurred.
    raise exc_info[0], exc_info[1], exc_info[2]


class StackedSetup(type):

  """Metaclass that extracts automatically stacks setUp and tearDown calls.

  Basically this exists to make it easier to do setUp *correctly*, while also
  suppressing some unittests misbehaviours- for example, the fact that if a
  setUp throws an exception the corresponding tearDown isn't ran.  This sorts
  it.

  Usage of it is via usual metaclass approach; just set
  `__metaclass__ = StackedSetup` .

  Note that this metaclass is designed such that because this is a metaclass,
  rather than just a scope mutator, all derivative classes derive from this
  metaclass; thus all derivative TestCase classes get automatic stacking."""
  def __new__(mcs, name, bases, scope):
    if 'setUp' in scope:
      scope['__raw_setUp__'] = scope.pop('setUp')
    scope['setUp'] = _stacked_setUp

    if 'tearDown' in scope:
      scope['__raw_tearDown__'] = scope.pop('tearDown')
    scope['tearDown'] = _stacked_tearDown

    return type.__new__(mcs, name, bases, scope)


class EasyAttr(dict):
  """Convenient class for simulating objects with attributes in tests.

  An EasyAttr object can be created with any attributes initialized very
  easily.  Examples:

  1) An object with .id=45 and .name="Joe":
  testobj = EasyAttr(id=45, name="Joe")
  2) An object with .title.text="Big" and .owner.text="Joe":
  testobj = EasyAttr(title=EasyAttr(text="Big"), owner=EasyAttr(text="Joe"))
  """

  __slots__ = ()

  def __getattr__(self, attr):
    try:
      return self[attr]
    except KeyError:
      return AttributeError(attr)

  def __delattr__(self, attr):
    try:
      self.pop(attr)
    except KeyError:
      raise AttributeError(attr)

  def __setattr__(self, attr, value):
    self[attr] = value

  def __dir__(self):
    return self.keys()


class LogFilter(logging.Filter):
  """A simple log filter that intercepts log messages and stores them."""

  def __init__(self):
    logging.Filter.__init__(self)
    self.messages = cStringIO.StringIO()

  def filter(self, record):
    self.messages.write(record.getMessage() + '\n')
    # Return False to prevent the message from being displayed.
    return False


class LoggingCapturer(object):
  """Captures all messages emitted by the logging module."""

  def __init__(self, logger_name=''):
    self._log_filter = LogFilter()
    self.logger_name = logger_name

  def __enter__(self):
    self.StartCapturing()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self.StopCapturing()

  def StartCapturing(self):
    """Begin capturing logging messages."""
    logging.getLogger(self.logger_name).addFilter(self._log_filter)


  def StopCapturing(self):
    """Stop capturing logging messages."""
    logging.getLogger(self.logger_name).removeFilter(self._log_filter)

  @property
  def messages(self):
    return self._log_filter.messages.getvalue()

  def LogsMatch(self, regex):
    """Checks whether the logs match a given regex."""
    match = re.search(regex, self.messages, re.MULTILINE)
    return match is not None

  def LogsContain(self, msg):
    """Checks whether the logs contain a given string."""
    return self.LogsMatch(re.escape(msg))


class OutputCapturer(object):
  """Class with limited support for capturing test stdout/stderr output.

  Class is designed as a 'ContextManager'.  Example usage in a test method
  of an object of TestCase:

  with self.OutputCapturer() as output:
    # Capturing of stdout/stderr automatically starts now.
    # Do stuff that sends output to stdout/stderr.
    # Capturing automatically stops at end of 'with' block.

  # stdout/stderr can be retrieved from the OutputCapturer object:
  stdout = output.getStdoutLines() # Or other access methods

  # Some Assert methods are only valid if capturing was used in test.
  self.AssertOutputContainsError() # Or other related methods
  """

  # These work with error output from operation module.
  OPER_MSG_SPLIT_RE = re.compile(r'^\033\[1;.*?\033\[0m$|^[^\n]*$',
                                 re.DOTALL | re.MULTILINE)
  ERROR_MSG_RE = re.compile(r'^\033\[1;%dm(.+?)(?:\033\[0m)+$' %
                            (30 + terminal.Color.RED,), re.DOTALL)
  WARNING_MSG_RE = re.compile(r'^\033\[1;%dm(.+?)(?:\033\[0m)+$' %
                              (30 + terminal.Color.YELLOW,), re.DOTALL)

  __slots__ = ['_stderr', '_stderr_cap', '_stdout', '_stdout_cap']

  def __init__(self):
    self._stdout = None
    self._stderr = None
    self._stdout_cap = None
    self._stderr_cap = None

  def __enter__(self):
    # This method is called with entering 'with' block.
    self.StartCapturing()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    # This method is called when exiting 'with' block.
    self.StopCapturing()

    if exc_type:
      print('Exception during output capturing: %r' % (exc_val,))
      stdout = self.GetStdout()
      if stdout:
        print('Captured stdout was:\n%s' % stdout)
      else:
        print('No captured stdout')
      stderr = self.GetStderr()
      if stderr:
        print('Captured stderr was:\n%s' % stderr)
      else:
        print('No captured stderr')

  def StartCapturing(self):
    """Begin capturing stdout and stderr."""
    self._stdout = sys.stdout
    self._stderr = sys.stderr
    sys.stdout = self._stdout_cap = cStringIO.StringIO()
    sys.stderr = self._stderr_cap = cStringIO.StringIO()

  def StopCapturing(self):
    """Stop capturing stdout and stderr."""
    # The only reason to check stdout or stderr separately might
    # have capturing on independently is if StartCapturing did not complete.
    if self._stdout:
      sys.stdout = self._stdout
      self._stdout = None
    if self._stderr:
      sys.stderr = self._stderr
      self._stderr = None

  def ClearCaptured(self):
    # Only valid if capturing is not on.
    assert self._stdout is None and self._stderr is None

    self._stdout_cap = None
    self._stderr_cap = None

  def GetStdout(self):
    """Return captured stdout so far."""
    return self._stdout_cap.getvalue()

  def GetStderr(self):
    """Return captured stderr so far."""
    return self._stderr_cap.getvalue()

  def _GetOutputLines(self, output, include_empties):
    """Split |output| into lines, optionally |include_empties|.

    Return array of lines.
    """

    lines = self.OPER_MSG_SPLIT_RE.findall(output)
    if not include_empties:
      lines = [ln for ln in lines if ln]

    return lines

  def GetStdoutLines(self, include_empties=True):
    """Return captured stdout so far as array of lines.

    If |include_empties| is false filter out all empty lines.
    """
    return self._GetOutputLines(self.GetStdout(), include_empties)

  def GetStderrLines(self, include_empties=True):
    """Return captured stderr so far as array of lines.

    If |include_empties| is false filter out all empty lines.
    """
    return self._GetOutputLines(self.GetStderr(), include_empties)


class TestCase(unittest.TestCase):

  __metaclass__ = StackedSetup

  # List of vars chromite is globally sensitive to and that should
  # be suppressed for tests.
  ENVIRON_VARIABLE_SUPPRESSIONS = ('CROS_CACHEDIR',)

  def __init__(self, *args, **kwds):
    unittest.TestCase.__init__(self, *args, **kwds)
    # This is set to keep pylint from complaining.
    self.__test_was_run__ = False

  def setUp(self):
    self.__saved_env__ = os.environ.copy()
    self.__saved_cwd__ = os.getcwd()
    self.__saved_umask__ = os.umask(022)
    for x in self.ENVIRON_VARIABLE_SUPPRESSIONS:
      os.environ.pop(x, None)

  def tearDown(self):
    osutils.SetEnvironment(self.__saved_env__)
    os.chdir(self.__saved_cwd__)
    os.umask(self.__saved_umask__)

  def assertRaises2(self, exception, functor, *args, **kwargs):
    """Like assertRaises, just with checking of the excpetion.

    args:
      exception: The expected exception type to intecept.
      functor: The function to invoke.
      args: Positional args to pass to the function.
      kwargs: Optional args to pass to the function.  Note we pull
        exact_kls, msg, and check_attrs from these kwargs.
      exact_kls: If given, the exception raise must be *exactly* that class
        type; derivatives are a failure.
      check_attrs: If given, a mapping of attribute -> value to assert on
        the resultant exception.  Thus if you wanted to catch a ENOENT, you
        would do:
          assertRaises2(EnvironmentError, func, args,
                        attrs={"errno":errno.ENOENT})
      msg: The error message to be displayed if the exception isn't raised.
        If not given, a suitable one is defaulted to.
      returns: The exception object.
    """
    exact_kls = kwargs.pop("exact_kls", None)
    check_attrs = kwargs.pop("check_attrs", {})
    msg = kwargs.pop("msg", None)
    if msg is None:
      msg = ("%s(*%r, **%r) didn't throw an exception"
             % (functor.__name__, args, kwargs))
    try:
      functor(*args, **kwargs)
      raise AssertionError(msg)
    except exception, e:
      if exact_kls:
        self.assertEqual(e.__class__, exception)
      bad = []
      for attr, required in check_attrs.iteritems():
        self.assertTrue(hasattr(e, attr),
                        msg="%s lacks attr %s" % (e, attr))
        value = getattr(e, attr)
        if value != required:
          bad.append("%s attr is %s, needed to be %s"
                     % (attr, value, required))
      if bad:
        raise AssertionError("\n".join(bad))
      return e

  def assertExists(self, path):
    """Make sure |path| exists"""
    if not os.path.exists(path):
      msg = ['path is missing: %s' % path]
      while path != '/':
        path = os.path.dirname(path)
        if not path:
          # If we're given something like "foo", abort once we get to "".
          break
        result = os.path.exists(path)
        msg.append('\tos.path.exists(%s): %s' % (path, result))
        if result:
          msg.append('\tcontents: %r' % os.listdir(path))
          break
      raise self.failureException('\n'.join(msg))


class LoggingTestCase(TestCase):
  """Base class for logging capturer test cases."""

  def AssertLogsMatch(self, log_capturer, regex, inverted=False):
    """Verifies a regex matches the logs."""
    assert_msg = '%r not found in %r' % (regex, log_capturer.messages)
    assert_fn = self.assertTrue
    if inverted:
      assert_msg = '%r found in %r' % (regex, log_capturer.messages)
      assert_fn = self.assertFalse

    assert_fn(log_capturer.LogsMatch(regex), msg=assert_msg)

  def AssertLogsContain(self, log_capturer, msg, inverted=False):
    """Verifies a message is contained in the logs."""
    return self.AssertLogsMatch(log_capturer, re.escape(msg), inverted=inverted)


class OutputTestCase(TestCase):
  """Base class for cros unit tests with utility methods."""

  def __init__(self, *args, **kwds):
    """Base class __init__ takes a second argument."""
    TestCase.__init__(self, *args, **kwds)
    self._output_capturer = None

  def OutputCapturer(self):
    """Create and return OutputCapturer object."""
    self._output_capturer = OutputCapturer()
    return self._output_capturer

  def _GetOutputCapt(self):
    """Internal access to existing OutputCapturer.

    Raises RuntimeError if output capturing was never on.
    """
    if self._output_capturer:
      return self._output_capturer

    raise RuntimeError('Output capturing was never turned on for this test.')

  def _GenCheckMsgFunc(self, prefix_re, line_re):
    """Return boolean func to check a line given |prefix_re| and |line_re|."""
    def _method(line):
      if prefix_re:
        # Prefix regexp will strip off prefix (and suffix) from line.
        match = prefix_re.search(line)

        if match:
          line = match.group(1)
        else:
          return False

      return line_re.search(line) if line_re else True

    # Provide a description of what this function looks for in a line.  Error
    # messages can make use of this.
    _method.description = None
    if prefix_re and line_re:
      _method.description = ('line matching prefix regexp %r then regexp %r' %
                             (prefix_re.pattern, line_re.pattern))
    elif prefix_re:
      _method.description = 'line matching prefix regexp %r' % prefix_re.pattern
    elif line_re:
      _method.description = 'line matching regexp %r' % line_re.pattern
    else:
      raise RuntimeError('Nonsensical usage of _GenCheckMsgFunc: '
                         'no prefix_re or line_re')

    return _method

  def _ContainsMsgLine(self, lines, msg_check_func):
    return any(msg_check_func(ln) for ln in lines)

  def _GenOutputDescription(self, check_stdout, check_stderr):
    # Some extra logic to make an error message useful.
    if check_stdout and check_stderr:
      return 'stdout or stderr'
    elif check_stdout:
      return 'stdout'
    elif check_stderr:
      return 'stderr'

  def _AssertOutputContainsMsg(self, check_msg_func, invert,
                               check_stdout, check_stderr):
    assert check_stdout or check_stderr

    lines = []
    if check_stdout:
      lines.extend(self._GetOutputCapt().GetStdoutLines())
    if check_stderr:
      lines.extend(self._GetOutputCapt().GetStderrLines())

    result = self._ContainsMsgLine(lines, check_msg_func)

    # Some extra logic to make an error message useful.
    output_desc = self._GenOutputDescription(check_stdout, check_stderr)

    if invert:
      msg = ('expected %s to not contain %s,\nbut found it in:\n%s' %
             (output_desc, check_msg_func.description, lines))
      self.assertFalse(result, msg=msg)
    else:
      msg = ('expected %s to contain %s,\nbut did not find it in:\n%s' %
             (output_desc, check_msg_func.description, lines))
      self.assertTrue(result, msg=msg)

  def AssertOutputContainsError(self, regexp=None, invert=False,
                                check_stdout=True, check_stderr=False):
    """Assert requested output contains at least one error line.

    If |regexp| is non-null, then the error line must also match it.
    If |invert| is true, then assert the line is NOT found.

    Raises RuntimeError if output capturing was never one for this test.
    """
    check_msg_func = self._GenCheckMsgFunc(OutputCapturer.ERROR_MSG_RE, regexp)
    return self._AssertOutputContainsMsg(check_msg_func, invert,
                                         check_stdout, check_stderr)

  def AssertOutputContainsWarning(self, regexp=None, invert=False,
                                  check_stdout=True, check_stderr=False):
    """Assert requested output contains at least one warning line.

    If |regexp| is non-null, then the warning line must also match it.
    If |invert| is true, then assert the line is NOT found.

    Raises RuntimeError if output capturing was never one for this test.
    """
    check_msg_func = self._GenCheckMsgFunc(OutputCapturer.WARNING_MSG_RE,
                                           regexp)
    return self._AssertOutputContainsMsg(check_msg_func, invert,
                                         check_stdout, check_stderr)

  def AssertOutputContainsLine(self, regexp, invert=False,
                               check_stdout=True, check_stderr=False):
    """Assert requested output contains line matching |regexp|.

    If |invert| is true, then assert the line is NOT found.

    Raises RuntimeError if output capturing was never one for this test.
    """
    check_msg_func = self._GenCheckMsgFunc(None, regexp)
    return self._AssertOutputContainsMsg(check_msg_func, invert,
                                         check_stdout, check_stderr)

  def _AssertOutputEndsInMsg(self, check_msg_func,
                             check_stdout, check_stderr):
    """Pass if requested output(s) ends(end) with an error message."""
    assert check_stdout or check_stderr

    lines = []
    if check_stdout:
      stdout_lines = self._GetOutputCapt().GetStdoutLines(include_empties=False)
      if stdout_lines:
        lines.append(stdout_lines[-1])
    if check_stderr:
      stderr_lines = self._GetOutputCapt().GetStderrLines(include_empties=False)
      if stderr_lines:
        lines.append(stderr_lines[-1])

    result = self._ContainsMsgLine(lines, check_msg_func)

    # Some extra logic to make an error message useful.
    output_desc = self._GenOutputDescription(check_stdout, check_stderr)

    msg = ('expected %s to end with %s,\nbut did not find it in:\n%s' %
           (output_desc, check_msg_func.description, lines))
    self.assertTrue(result, msg=msg)

  def AssertOutputEndsInError(self, regexp=None,
                              check_stdout=True, check_stderr=False):
    """Assert requested output ends in error line.

    If |regexp| is non-null, then the error line must also match it.

    Raises RuntimeError if output capturing was never one for this test.
    """
    check_msg_func = self._GenCheckMsgFunc(OutputCapturer.ERROR_MSG_RE, regexp)
    return self._AssertOutputEndsInMsg(check_msg_func,
                                       check_stdout, check_stderr)

  def AssertOutputEndsInWarning(self, regexp=None,
                                check_stdout=True, check_stderr=False):
    """Assert requested output ends in warning line.

    If |regexp| is non-null, then the warning line must also match it.

    Raises RuntimeError if output capturing was never one for this test.
    """
    check_msg_func = self._GenCheckMsgFunc(OutputCapturer.WARNING_MSG_RE,
                                           regexp)
    return self._AssertOutputEndsInMsg(check_msg_func,
                                       check_stdout, check_stderr)

  def AssertOutputEndsInLine(self, regexp,
                             check_stdout=True, check_stderr=False):
    """Assert requested output ends in line matching |regexp|.

    Raises RuntimeError if output capturing was never one for this test.
    """
    check_msg_func = self._GenCheckMsgFunc(None, regexp)
    return self._AssertOutputEndsInMsg(check_msg_func,
                                       check_stdout, check_stderr)

  def FuncCatchSystemExit(self, func, *args, **kwargs):
    """Run |func| with |args| and |kwargs| and catch exceptions.SystemExit.

    Return tuple (return value or None, SystemExit number code or None).
    """
    try:
      returnval = func(*args, **kwargs)

      return returnval, None
    except exceptions.SystemExit as ex:
      exit_code = ex.args[0]
      return None, exit_code

  def AssertFuncSystemExitZero(self, func, *args, **kwargs):
    """Run |func| with |args| and |kwargs| catching exceptions.SystemExit.

    If the func does not raise a SystemExit with exit code 0 then assert.
    """
    exit_code = self.FuncCatchSystemExit(func, *args, **kwargs)[1]
    self.assertFalse(exit_code is None,
                      msg='Expected system exit code 0, but caught none')
    self.assertTrue(exit_code == 0,
                    msg='Expected system exit code 0, but caught %d' %
                    exit_code)

  def AssertFuncSystemExitNonZero(self, func, *args, **kwargs):
    """Run |func| with |args| and |kwargs| catching exceptions.SystemExit.

    If the func does not raise a non-zero SystemExit code then assert.
    """
    exit_code = self.FuncCatchSystemExit(func, *args, **kwargs)[1]
    self.assertFalse(exit_code is None,
                      msg='Expected non-zero system exit code, but caught none')
    self.assertFalse(exit_code == 0,
                     msg='Expected non-zero system exit code, but caught %d' %
                     exit_code)

  def AssertRaisesAndReturn(self, error, func, *args, **kwargs):
    """Like assertRaises, but return exception raised."""
    try:
      func(*args, **kwargs)
      self.assertTrue(False, msg='Expected %s but got none' % error)
    except error as ex:
      return ex


class TempDirTestCase(TestCase):
  """Mixin used to give each test a tempdir that is cleansed upon finish"""

  sudo_cleanup = False

  def __init__(self, *args, **kwds):
    TestCase.__init__(self, *args, **kwds)
    self.tempdir = None
    self._tempdir_obj = None

  def setUp(self):
    self._tempdir_obj = osutils.TempDir(prefix='chromite.test', set_global=True)
    self.tempdir = self._tempdir_obj.tempdir

  def tearDown(self):
    if self._tempdir_obj is not None:
      self._tempdir_obj.Cleanup()
      self.tempdir = None
      self._tempdir_obj = None


class GerritTestCase(TempDirTestCase):
  """Test class for tests that interact with a gerrit server.

  The class setup creates and launches a stand-alone gerrit instance running on
  localhost, for test methods to interact with.  Class teardown stops and
  deletes the gerrit instance.

  Note that there is a single gerrit instance for ALL test methods in a
  GerritTestCase sub-class.
  """

  GerritInstance = collections.namedtuple('GerritInstance', [
      'credential_file',
      'gerrit_dir',
      'gerrit_exe',
      'gerrit_host',
      'gerrit_pid',
      'gerrit_url',
      'git_dir',
      'git_host',
      'git_url',
      'http_port',
      'netrc_file',
      'ssh_port',
  ])

  @classmethod
  def _create_gerrit_instance(cls, gerrit_dir):
    gerrit_init_script = os.path.join(
        cros_build_lib.FindDepotTools(), 'testing_support', 'gerrit-init.sh')
    http_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    http_sock.bind(('', 0))
    http_port = str(http_sock.getsockname()[1])
    ssh_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ssh_sock.bind(('', 0))
    ssh_port = str(ssh_sock.getsockname()[1])

    # NOTE: this is not completely safe.  These port numbers could be
    # re-assigned by the OS between the calls to socket.close() and gerrit
    # starting up.  The only safe way to do this would be to pass file
    # descriptors down to the gerrit process, which is not even remotely
    # supported.  Alas.
    http_sock.close()
    ssh_sock.close()
    cros_build_lib.RunCommandQuietly(
        ['bash', gerrit_init_script, '--http-port', http_port,
         '--ssh-port', ssh_port, gerrit_dir])

    gerrit_exe = os.path.join(gerrit_dir, 'bin', 'gerrit.sh')
    cros_build_lib.RunCommandQuietly(['bash', gerrit_exe, 'start'])
    gerrit_pid = int(osutils.ReadFile(
        os.path.join(gerrit_dir, 'logs', 'gerrit.pid')).rstrip())
    with open(os.path.join(gerrit_dir, 'logs', 'gerrit.pid')) as fh:
      gerrit_pid = int(fh.read().rstrip())
    return cls.GerritInstance(
        credential_file=os.path.join(gerrit_dir, 'tmp', '.git-credentials'),
        gerrit_dir=gerrit_dir,
        gerrit_exe=gerrit_exe,
        gerrit_host='localhost:%s' % http_port,
        gerrit_pid=gerrit_pid,
        gerrit_url='http://localhost:%s' % http_port,
        git_dir=os.path.join(gerrit_dir, 'git'),
        git_host='%s/git' % gerrit_dir,
        git_url='file://%s/git' % gerrit_dir,
        http_port=http_port,
        netrc_file=os.path.join(gerrit_dir, 'tmp', '.netrc'),
        ssh_port=ssh_port,)

  @classmethod
  def setUpClass(cls):
    """Sets up the gerrit instances in a class-specific temp dir."""
    # Create gerrit instance.
    cls.gerritdir_obj = osutils.TempDir(set_global=False)
    gi = cls.gerrit_instance = cls._create_gerrit_instance(
        cls.gerritdir_obj.tempdir)

    # Set netrc file for http authentication.
    cls.netrc_patcher = mock.patch('chromite.lib.gob_util.NETRC',
                                   netrc.netrc(gi.netrc_file))
    cls.netrc_patcher.start()

    # gob_util only knows about https connections, and that's a good thing.
    # But for testing, it's much simpler to use http connections.
    cls.httplib_patcher = mock.patch(
        'httplib.HTTPSConnection', httplib.HTTPConnection)
    cls.httplib_patcher.start()
    cls.protocol_patcher = mock.patch(
        'chromite.lib.gob_util.GERRIT_PROTOCOL', 'http')
    cls.protocol_patcher.start()

    # Make all chromite code point to the test server.
    cls.constants_patcher = mock.patch.dict(constants.__dict__, {
        'PUBLIC_GOB_HOST': gi.git_host,
        'PUBLIC_GERRIT_HOST': gi.gerrit_host,
        'PUBLIC_GOB_URL': gi.git_url,
        'PUBLIC_GERRIT_URL': gi.gerrit_url,
        'INTERNAL_GOB_HOST': gi.git_host,
        'INTERNAL_GERRIT_HOST': gi.gerrit_host,
        'INTERNAL_GOB_URL': gi.git_url,
        'INTERNAL_GERRIT_URL': gi.gerrit_url,
        'GERRIT_HOST': gi.gerrit_host,
        'GERRIT_INT_HOST': gi.gerrit_host,
        'GIT_HOST': gi.git_host,
        'GERRIT_SSH_URL': gi.gerrit_url,
        'GERRIT_INT_SSH_URL': gi.gerrit_url,
        'GIT_HTTP_URL': gi.git_url,
        'MANIFEST_URL': '%s/%s' % (gi.git_url, constants.MANIFEST_PROJECT),
        'MANIFEST_INT_URL': '%s/%s' % (
            gi.git_url, constants.MANIFEST_INT_PROJECT),
        'GIT_REMOTES': {
            constants.EXTERNAL_REMOTE: gi.gerrit_url,
            constants.INTERNAL_REMOTE: gi.gerrit_url,
            constants.CHROMIUM_REMOTE: gi.gerrit_url,
            constants.CHROME_REMOTE: gi.gerrit_url,
        }
    })
    cls.constants_patcher.start()

  def createProject(self, name, description='Test project', owners=None,
                    submit_type='CHERRY_PICK'):
    """Create a project on the test gerrit server."""
    if owners is None:
      owners = ['Administrators']
    body = {
        'description': description,
        'submit_type': submit_type,
        'owners': owners,
    }
    path = 'projects/%s' % name
    conn = gob_util.CreateHttpConn(
        self.gerrit_instance.gerrit_host, path, reqtype='PUT', body=body)
    response = conn.getresponse()
    self.assertEqual(response.status, 201, response.reason)
    s = cStringIO.StringIO(response.read())
    self.assertEqual(s.readline().rstrip(), ")]}'")
    jmsg = json.load(s)
    self.assertEqual(jmsg['name'], name)

  def cloneProject(self, name, path=None):
    """Clone a project from the test gerrit server."""
    if path is None:
      path = os.path.basename(name)
      if path.endswith('.git'):
        path = path[:-4]
    path = os.path.join(self.tempdir, path)
    osutils.SafeMakedirs(os.path.dirname(path))
    url = 'http://%s/%s' % (self.gerrit_instance.gerrit_host, name)
    git.RunGit(os.getcwd(), ['clone', url, path])
    # Install commit-msg hook.
    hook_path = os.path.join(path, '.git', 'hooks', 'commit-msg')
    cros_build_lib.RunCommandQuietly(
        ['curl', '-o', hook_path,
         'http://%s/tools/hooks/commit-msg' % self.gerrit_instance.gerrit_host])
    os.chmod(hook_path, stat.S_IRWXU)
    # Configure non-interactive credentials for git operations.
    config_path = os.path.join(path, '.git', 'config')
    cros_build_lib.RunCommandQuietly(
        ['git', 'config', '--file', config_path, 'credential.helper',
         'store --file=%s' % self.gerrit_instance.credential_file])
    return path

  def createCommit(self, clone_path, fn='test-file.txt'):
    """Create a commit in the given git checkout."""
    clone_path = os.path.join(self.tempdir, clone_path)
    fpath = os.path.join(clone_path, fn)
    osutils.WriteFile(fpath, 'Another day, another dollar.\n', mode='a')
    cros_build_lib.RunCommandQuietly(['git', 'add', fn], cwd=clone_path)
    cros_build_lib.RunCommandQuietly(
        ['git', 'commit', '-m', 'Test commit'], cwd=clone_path)
    return self.getHeadCommit(clone_path)

  def getHeadCommit(self, clone_path):
    """Get the sha1 and change-id for the head commit in a git checkout."""
    clone_path = os.path.join(self.tempdir, clone_path)
    log_proc = cros_build_lib.RunCommandCaptureOutput(
        ['git', 'log', '-n', '1'], cwd=clone_path, print_cmd=False)
    sha1 = None
    change_id = None
    for line in log_proc.output.splitlines():
      match = re.match(r'^commit ([0-9a-fA-F]{40})$', line)
      if match:
        sha1 = match.group(1)
        continue
      match = re.match('^\s+Change-Id:\s*(\S+)$', line)
      if match:
        change_id = match.group(1)
        continue
    self.assertTrue(sha1)
    self.assertTrue(change_id)
    return (sha1, change_id)

  def uploadChange(self, clone_path, branch='master'):
    """Create a gerrit CL from the HEAD of a git checkout."""
    clone_path = os.path.join(self.tempdir, clone_path)
    cros_build_lib.RunCommandQuietly(
        ['git', 'push', 'origin', 'HEAD:refs/for/%s' % branch], cwd=clone_path)

  def pushBranch(self, clone_path, branch='master'):
    clone_path = os.path.join(self.tempdir, clone_path)
    cros_build_lib.RunCommandQuietly(
        ['git', 'push', 'origin', 'HEAD:refs/heads/%s' % branch], cwd=clone_path)

  @staticmethod
  def _stop_gerrit(gerrit_obj):
    """Stops the running gerrit instance and deletes it."""
    try:
      # This should terminate the gerrit process.
      cros_build_lib.RunCommandQuietly(['bash', gerrit_obj.gerrit_exe, 'stop'])
    finally:
      try:
        # cls.gerrit_pid should have already terminated.  If it did, then
        # os.waitpid will raise OSError.
        os.waitpid(gerrit_obj.gerrit_pid, os.WNOHANG)
      except OSError as e:
        if e.errno == errno.ECHILD:
          # If gerrit shut down cleanly, os.waitpid will land here.
          return

      # If we get here, the gerrit process is still alive.  Send the process
      # SIGKILL for good measure.
      try:
        os.kill(gerrit_obj.gerrit_pid, signal.SIGKILL)
      except OSError:
        if e.errno == errno.ESRCH:
          # os.kill raised an error because the process doesn't exist.  Maybe
          # gerrit shut down cleanly after all.
          return

      # Expose the fact that gerrit didn't shut down cleanly.
      cros_build_lib.Warning(
          'Test gerrit server (pid=%d) did not shut down cleanly.' % (
              gerrit_obj.gerrit_pid))

  @classmethod
  def tearDownClass(cls):
    cls.netrc_patcher.stop()
    cls.httplib_patcher.stop()
    cls.protocol_patcher.stop()
    cls.constants_patcher.stop()

    cls._stop_gerrit(cls.gerrit_instance)
    cls.gerritdir_obj.Cleanup()


class GerritInternalTestCase(GerritTestCase):
  """Test class which runs separate internal and external gerrit instances."""

  @classmethod
  def setUpClass(cls):
    cls.int_gerritdir_obj = osutils.TempDir(set_global=False)
    pgi = cls.gerrit_instance
    igi = cls.int_gerrit_instance = cls._create_gerrit_instance(
        cls.int_gerritdir_obj.tempdir)
    cls.int_constants_patcher = mock.patch.dict(constants.__dict__, {
        'INTERNAL_GOB_HOST': igi.git_host,
        'INTERNAL_GERRIT_HOST': igi.gerrit_host,
        'INTERNAL_GOB_URL': igi.git_url,
        'INTERNAL_GERRIT_URL': igi.gerrit_url,
        'GERRIT_INT_SSH_URL': igi.gerrit_url,
        'MANIFEST_INT_URL': '%s/%s' % (
            igi.git_url, constants.MANIFEST_INT_PROJECT),
        'GIT_REMOTES': {
            constants.EXTERNAL_REMOTE: pgi.gerrit_url,
            constants.INTERNAL_REMOTE: igi.gerrit_url,
            constants.CHROMIUM_REMOTE: pgi.gerrit_url,
            constants.CHROME_REMOTE: igi.gerrit_url,
        }
    })
    cls.int_constants_patcher.start()

  @classmethod
  def tearDownClass(cls):
    cls.int_constants_patcher.stop()
    cls._stop_gerrit(cls.int_gerrit_instance)
    cls.int_gerritdir_obj.Cleanup()


class _RunCommandMock(mox.MockObject):
  """Custom mock class used to suppress arguments we don't care about"""

  DEFAULT_IGNORED_ARGS = ('print_cmd',)

  def __call__(self, *args, **kwds):
    for arg in self.DEFAULT_IGNORED_ARGS:
      kwds.setdefault(arg, mox.IgnoreArg())
    return mox.MockObject.__call__(self, *args, **kwds)


class _LessAnnoyingMox(mox.Mox):
  """Mox derivative that slips in our suppressions to mox.

  This is used by default via MoxTestCase; namely, this suppresses
  certain arguments awareness that we don't care about via switching
  in (dependent on the namespace requested) overriding MockObject
  classing.

  Via this, it makes maintenance much simpler- simplest example, if code
  doesn't explicitly assert that print_cmd must be true/false... then
  we don't care about what argument is set (it has no effect beyond output).
  Mox normally *would* care, making it a pita to maintain.  This selectively
  suppresses that awareness, making it maintainable.
  """

  mock_classes = {}.fromkeys(
      ['chromite.lib.cros_build_lib.%s' % x
       for x in dir(cros_build_lib) if "RunCommand" in x],
       _RunCommandMock)

  @staticmethod
  def _GetNamespace(obj):
    return '%s.%s' % (obj.__module__, obj.__name__)

  def CreateMock(self, obj, attrs=None):
    if attrs is None:
      attrs = {}
    kls = self.mock_classes.get(
        self._GetNamespace(obj), mox.MockObject)
    # Copy attrs; I don't trust mox to not be stupid here.
    new_mock = kls(obj, attrs=attrs)
    self._mock_objects.append(new_mock)
    return new_mock


class MoxTestCase(TestCase):
  """Mox based test case; compatible with StackedSetup

  Note: mox is deprecated; please use MockTestCase instead.
  """

  mox_suppress_verify_all = False

  def setUp(self):
    self.mox = _LessAnnoyingMox()
    self.stubs = mox.stubout.StubOutForTesting()

  def tearDown(self):
    try:
      if self.__test_was_run__ and not self.mox_suppress_verify_all:
        # This means the test code was actually ran.
        # force a verifyall
        self.mox.VerifyAll()
    finally:
      if hasattr(self, 'mox'):
        self.mox.UnsetStubs()
      if hasattr(self, 'stubs'):
        self.stubs.UnsetAll()
        self.stubs.SmartUnsetAll()


class MoxTempDirTestCase(TempDirTestCase, MoxTestCase):
  """Convenience class mixing TempDir and Mox

  Note: mox is deprecated; please use MockTempDirTestCase instead.
  """


class MoxOutputTestCase(OutputTestCase, MoxTestCase):
  """Conevenience class mixing OutputTestCase and MoxTestCase

  Note: mox is deprecated; please use MockOutputTestCase instead.
  """


class MockTestCase(TestCase):
  """Python-mock based test case; compatible with StackedSetup"""
  def setUp(self):
    self._patchers = []

  def tearDown(self):
    # We can't just run stopall() by itself, and need to stop our patchers
    # manually since stopall() doesn't handle repatching.
    cros_build_lib.SafeRun([p.stop for p in reversed(self._patchers)] +
                           [mock.patch.stopall])

  def StartPatcher(self, patcher):
    """Call start() on the patcher, and stop() in tearDown."""
    m = patcher.start()
    self._patchers.append(patcher)
    return m

  def PatchObject(self, *args, **kwargs):
    """Create and start a mock.patch.object().

    stop() will be called automatically during tearDown.
    """
    return self.StartPatcher(mock.patch.object(*args, **kwargs))


# MockTestCase must be before TempDirTestCase in this inheritance order,
# because MockTestCase.StartPatcher() calls may be for PartialMocks, which
# create their own temporary directory.  The teardown for those directories
# occurs during MockTestCase.tearDown(), which needs to be run before
# TempDirTestCase.tearDown().
class MockTempDirTestCase(MockTestCase, TempDirTestCase):
  """Convenience class mixing TempDir and Mock."""


class MockOutputTestCase(MockTestCase, OutputTestCase):
  """Convenience class mixing Output and Mock."""


class MockLoggingTestCase(MockTestCase, LoggingTestCase):
  """Convenience class mixing Logging and Mock."""


def FindTests(directory, module_namespace=''):
  """Find all *_unittest.py, and return their python namespaces.

  Args:
    directory: The directory to scan for tests.
    module_namespace: What namespace to prefix all found tests with.
  Returns:
    A list of python unittests in python namespace form.
  """
  results = cros_build_lib.RunCommandCaptureOutput(
      ['find', '.', '-name', '*_unittest.py', '-printf', '%P\n'],
      cwd=directory, print_cmd=False).output.splitlines()
  # Drop the trailing .py, inject in the name if one was given.
  if module_namespace:
    module_namespace += '.'
  return [module_namespace + x[:-3].replace('/', '.') for x in results]


def main(**kwds):
  """Helper wrapper around unittest.main.  Invoke this, not unittest.main.

  Any passed in kwds are passed directly down to unittest.main; via this, you
  can inject custom argv for example (to limit what tests run).
  """
  # Default to exit=True; this matches old behaviour, and allows unittest
  # to trigger sys.exit on its own.  Unfortunately, the exit keyword is only
  # available in 2.7- as such, handle it ourselves.
  allow_exit = kwds.pop('exit', True)
  if '--network' in sys.argv:
    sys.argv.remove('--network')
    GlobalTestConfig.NETWORK_TESTS_DISABLED = False
  level = kwds.pop('level', logging.CRITICAL)
  for flag in ('-d', '--debug'):
    if flag in sys.argv:
      sys.argv.remove(flag)
      level = logging.DEBUG
  cros_build_lib.SetupBasicLogging(level)
  try:
    unittest.main(**kwds)
    raise SystemExit(0)
  except SystemExit, e:
    if e.__class__ != SystemExit or allow_exit:
      raise
    # Redo the exit code ourselves- unittest throws True on occasion.
    # This is why the lack of typing for SystemExit code attribute makes life
    # suck, in parallel to unittest being special.
    # Finally, note that it's possible for code to be a string...
    if isinstance(e.code, (int, long)):
      # This is done since exit code may be something other than 1/0; if they
      # explicitly pass it, we'll honor it.
      return e.code
    return 1 if e.code else 0
