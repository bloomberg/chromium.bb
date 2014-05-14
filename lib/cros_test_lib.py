#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cros unit test library, with utility functions."""

from __future__ import print_function
import collections
import cookielib
import cStringIO
import datetime
import exceptions
import functools
import hashlib
import json
import logging
import mox
import netrc
import os
import re
import socket
import stat
import sys
import unittest
import urllib

from chromite.cbuildbot import constants
import cros_build_lib
import gob_util
import osutils
import terminal
import timeout_util

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

  Args:
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

  Args:
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

  Args:
    base_path: See CreateOnDiskHierarchy()
    dir_struct: See CreateOnDiskHierarchy()

  Raises:
    AssertionError when there is any divergence between the on-disk
    structure and the structure specified by 'dir_struct'.
  """
  expected = _FlattenStructure(base_path, dir_struct)
  _VerifyDirectoryIterables(osutils.DirectoryIterator(base_path), expected)


def VerifyTarball(tarball, dir_struct):
  """Compare the contents of a tarball against a directory structure.

  Args:
    tarball: Path to the tarball.
    dir_struct: See CreateOnDiskHierarchy()

  Raises:
    AssertionError when there is any divergence between the tarball and the
    structure specified by 'dir_struct'.
  """
  contents = cros_build_lib.RunCommand(
      ['tar', '-tf', tarball], capture_output=True).output.splitlines()
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


class StackedSetup(type):
  """Metaclass to simplify unit testing and make it more robust.

  A metaclass alters the way that classes are initialized, enabling us to
  modify the class dictionary prior to the class being created. We use this
  feature here to modify the way that unit tests work a bit.

  This class does three things:
    1) When a test case is set up or torn down, we now run all setUp and
       tearDown methods in the inheritance tree.
    2) If a setUp or tearDown method fails, we still run tearDown methods
       for any test classes that were partially or completely set up.
    3) All test cases time out after TEST_CASE_TIMEOUT seconds.

  To use this class, set the following in your class:
    __metaclass__ = StackedSetup

  Since cros_test_lib.TestCase uses this metaclass, all derivatives of TestCase
  also inherit the above behavior (unless they override the __metaclass__
  attribute manually.)
  """

  TEST_CASE_TIMEOUT = 10 * 60

  def __new__(mcs, name, bases, scope):
    """Generate the new class with pointers to original funcs & our helpers"""
    if 'setUp' in scope:
      scope['__raw_setUp__'] = scope.pop('setUp')
    scope['setUp'] = mcs._stacked_setUp

    if 'tearDown' in scope:
      scope['__raw_tearDown__'] = scope.pop('tearDown')
    scope['tearDown'] = mcs._stacked_tearDown

    # Modify all test* methods to time out after TEST_CASE_TIMEOUT seconds.
    timeout = scope.get('TEST_CASE_TIMEOUT', StackedSetup.TEST_CASE_TIMEOUT)
    if timeout is not None:
      for name, func in scope.iteritems():
        if name.startswith('test') and hasattr(func, '__call__'):
          wrapper = timeout_util.TimeoutDecorator(timeout)
          scope[name] = wrapper(func)

    return type.__new__(mcs, name, bases, scope)

  @staticmethod
  def _walk_mro_stacking(obj, attr, reverse=False):
    """Walk the stacked classes (python method resolution order)"""
    iterator = iter if reverse else reversed
    methods = (getattr(x, attr, None) for x in iterator(obj.__class__.__mro__))
    seen = set()
    for x in filter(None, methods):
      x = getattr(x, 'im_func', x)
      if x not in seen:
        seen.add(x)
        yield x

  @staticmethod
  def _stacked_setUp(obj):
    """Run all the setUp funcs; if any fail, run all the tearDown funcs"""
    obj.__test_was_run__ = False
    try:
      for target in StackedSetup._walk_mro_stacking(obj, '__raw_setUp__'):
        target(obj)
    except:
      # TestCase doesn't trigger tearDowns if setUp failed; thus
      # manually force it ourselves to ensure cleanup occurs.
      StackedSetup._stacked_tearDown(obj)
      raise

    # Now mark the object as fully setUp; this is done so that
    # any last minute assertions in tearDown can know if they should
    # run or not.
    obj.__test_was_run__ = True

  @staticmethod
  def _stacked_tearDown(obj):
    """Run all the tearDown funcs; if any fail, we move on to the next one"""
    exc_info = None
    for target in StackedSetup._walk_mro_stacking(obj, '__raw_tearDown__',
                                                  True):
      #pylint: disable=W0702
      try:
        target(obj)
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


class TruthTable(object):
  """Class to represent a boolean truth table, useful in unit tests.

  If you find yourself testing the behavior of some function that should
  basically follow the behavior of a particular truth table, then this class
  can allow you to fully test that function without being overly verbose
  in the unit test code.

  The following usage is supported on a constructed TruthTable:
  1) Iterate over input lines of the truth table, expressed as tuples of
  bools.
  2) Access a particular input line by index, expressed as a tuple of bools.
  3) Access the expected output for a set of inputs.

  For example, say function "Foo" in module "mod" should consists of the
  following code:

  def Foo(A, B, C):
    return A and B and not C

  In the unittest for Foo, do this:

  def testFoo(self):
    truth_table = cros_test_lib.TruthTable(inputs=[(True, True, True)])
    for inputs in truth_table:
      a, b, c = inputs
      result = mod.Foo(a, b, c)
      self.assertEquals(result, truth_table.GetOutput(inputs))
  """

  class TruthTableInputIterator(object):
    """Class to support iteration over inputs of a TruthTable."""
    def __init__(self, truth_table):
      self.truth_table = truth_table
      self.next_line = 0

    def __iter__(self):
      return self

    def __next__(self):
      return self.next()

    def next(self):
      if self.next_line < self.truth_table.num_lines:
        self.next_line += 1
        return self.truth_table.GetInputs(self.next_line - 1)
      else:
        raise StopIteration()

  def __init__(self, inputs, input_result=True):
    """Construct a TruthTable from given inputs.

    Args:
      inputs: Iterable of input lines, each expressed as a tuple of bools.
        Each tuple must have the same length.
      input_result: The output intended for each specified input.  For
        truth tables that mostly output True it is more concise to specify
        the false inputs and then set input_result to False.
    """
    # At least one input required.
    if not inputs:
      raise ValueError('Inputs required to construct TruthTable.')

    # Save each input tuple in a set.  Also confirm that the length
    # of each input tuple is the same.
    self.dimension = len(inputs[0])
    self.num_lines = pow(2, self.dimension)
    self.expected_inputs = set()
    self.expected_inputs_result = input_result

    for input_vals in inputs:
      if len(input_vals) != self.dimension:
        raise ValueError('All TruthTable inputs must have same dimension.')

      self.expected_inputs.add(input_vals)

    # Start generator index at 0.
    self.next_line = 0

  def __len__(self):
    return self.num_lines

  def __iter__(self):
    return self.TruthTableInputIterator(self)

  def GetInputs(self, inputs_index):
    """Get the input line at the given input index.

    Args:
      inputs_index: Following must hold: 0 <= inputs_index < self.num_lines.

    Returns:
      Tuple of bools representing one line of inputs.
    """
    if inputs_index >= 0 and inputs_index < self.num_lines:
      line_values = []

      # Iterate through each column in truth table.  Any order will
      # produce a valid truth table, but going backward through
      # columns will produce the traditional truth table ordering.
      # For 2-dimensional example: F,F then F,T then T,F then T,T.
      for col in xrange(self.dimension - 1, -1, -1):
        line_values.append(bool(inputs_index / pow(2, col) % 2))

      return tuple(line_values)

    raise ValueError('This truth table has no line at index %r.' % inputs_index)

  def GetOutput(self, inputs):
    """Get the boolean output for the given inputs.

    Args:
      inputs: Tuple of bools, length must be equal to self.dimension.

    Returns:
      bool value representing truth table output for given inputs.
    """
    if not isinstance(inputs, tuple):
      raise TypeError('Truth table inputs must be specified as a tuple.')

    if not len(inputs) == self.dimension:
      raise ValueError('Truth table inputs must match table dimension.')

    return self.expected_inputs_result == (inputs in self.expected_inputs)


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
      raise AttributeError(attr)

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
  stdout = output.GetStdoutLines() # Or other access methods

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
    self._stdout = mock.patch('sys.stdout', new_callable=cStringIO.StringIO)
    self._stderr = mock.patch('sys.stderr', new_callable=cStringIO.StringIO)
    self._stderr_cap = self._stdout_cap = None

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
    self._stdout_cap = self._stdout.start()
    self._stderr_cap = self._stderr.start()

  def StopCapturing(self):
    """Stop capturing stdout and stderr."""
    self._stdout.stop()
    self._stderr.stop()

  def ClearCaptured(self):
    """Clear any captured stdout/stderr content."""
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
  """Basic chromite test case.

  Provides sane setUp/tearDown logic so that tearDown is correctly cleaned up.

  Takes care of saving/restoring process-wide settings like the environment so
  that sub-tests don't have to worry about gettings this right.

  Also includes additional assert helpers beyond python stdlib.
  """

  __metaclass__ = StackedSetup

  # List of vars chromite is globally sensitive to and that should
  # be suppressed for tests.
  ENVIRON_VARIABLE_SUPPRESSIONS = ('CROS_CACHEDIR',)

  def __init__(self, *args, **kwargs):
    unittest.TestCase.__init__(self, *args, **kwargs)
    # This is set to keep pylint from complaining.
    self.__test_was_run__ = False

  def setUp(self):
    self.__saved_env__ = os.environ.copy()
    self.__saved_cwd__ = os.getcwd()
    self.__saved_umask__ = os.umask(0o22)
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
    except exception as e:
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

  def assertNotExists(self, path):
    """Make sure |path| does not exist"""
    if os.path.exists(path):
      raise self.failureException('path exists when it should not: %s' % path)


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

  def __init__(self, *args, **kwargs):
    """Base class __init__ takes a second argument."""
    TestCase.__init__(self, *args, **kwargs)
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

    if isinstance(prefix_re, str):
      prefix_re = re.compile(prefix_re)
    if isinstance(line_re, str):
      line_re = re.compile(line_re)

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

  def __init__(self, *args, **kwargs):
    TestCase.__init__(self, *args, **kwargs)
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


class GerritTestCase(MockTempDirTestCase):
  """Test class for tests that interact with a Gerrit server.

  Configured by default to use a specially-configured test Gerrit server at
  t3st-chr0m3(-review).googlesource.com. The test server configuration may be
  altered by setting the following environment variables from the parent
  process:
    CROS_TEST_GIT_HOST: host name for git operations; defaults to
                        t3st-chr0me.googlesource.com.
    CROS_TEST_GERRIT_HOST: host name for Gerrit operations; defaults to
                           t3st-chr0me-review.googlesource.com.
    CROS_TEST_COOKIES_PATH: path to a cookies.txt file to use for git/Gerrit
                            requests; defaults to none.
    CROS_TEST_COOKIE_NAMES: comma-separated list of cookie names from
                            CROS_TEST_COOKIES_PATH to set on requests; defaults
                            to none. The current implementation only sends
                            cookies matching the exact host name and the empty
                            path ("/").
  """

  TEST_USERNAME = 'test-username'
  TEST_EMAIL = 'test-username@test.org'

  GerritInstance = collections.namedtuple('GerritInstance', [
      'cookie_names',
      'cookies_path',
      'gerrit_host',
      'gerrit_url',
      'git_host',
      'git_url',
      'netrc_file',
      'project_prefix',
  ])

  def _create_gerrit_instance(self, tmp_dir):
    default_host = 't3st-chr0m3'
    git_host = os.environ.get('CROS_TEST_GIT_HOST',
                              '%s.googlesource.com' % default_host)
    gerrit_host = os.environ.get('CROS_TEST_GERRIT_HOST',
                                 '%s-review.googlesource.com' % default_host)
    ip = socket.gethostbyname(socket.gethostname())
    project_prefix = 'test-%s-%s/' % (
        datetime.datetime.now().strftime('%Y%m%d%H%M%S'),
        hashlib.sha1('%s_%s' % (ip, os.getpid())).hexdigest()[:8])
    cookies_path = os.environ.get('CROS_TEST_COOKIES_PATH')
    cookie_names_str = os.environ.get('CROS_TEST_COOKIE_NAMES', '')
    cookie_names = [c for c in cookie_names_str.split(',') if c]

    return self.GerritInstance(
        cookie_names=cookie_names,
        cookies_path=cookies_path,
        gerrit_host=gerrit_host,
        gerrit_url='https://%s/' % gerrit_host,
        git_host=git_host,
        git_url='https://%s/' % git_host,
        # TODO(dborowitz): Ensure this is populated when using role account.
        netrc_file=os.path.join(tmp_dir, '.netrc'),
        project_prefix=project_prefix,)

  def _populate_netrc(self, src):
    """Sets up a test .netrc file using the given source as a base."""
    # Heuristic: prefer passwords for @google.com accounts, since test host
    # permissions tend to refer to those accounts.
    preferred_account_domains = ['.google.com']
    needed = [self.gerrit_instance.git_host, self.gerrit_instance.gerrit_host]
    candidates = collections.defaultdict(list)
    src_netrc = netrc.netrc(src)
    for host, v in src_netrc.hosts.iteritems():
      dot = host.find('.')
      if dot < 0:
        continue
      for n in needed:
        if n.endswith(host[dot:]):
          login, _, password = v
          i = 1
          for pd in preferred_account_domains:
            if login.endswith(pd):
              i = 0
              break
          candidates[n].append((i, login, password))

    with open(self.gerrit_instance.netrc_file, 'w') as out:
      for n in needed:
        cs = candidates[n]
        self.assertTrue(len(cs) > 0, 'missing password in ~/.netrc for %s' % n)
        cs.sort()
        _, login, password = cs[0]
        out.write('machine %s login %s password %s\n' % (n, login, password))

  def setUp(self):
    """Sets up the gerrit instances in a class-specific temp dir."""
    # Create gerrit instance.
    gi = self.gerrit_instance = self._create_gerrit_instance(self.tempdir)
    old_home = os.environ['HOME']
    os.environ['HOME'] = self.tempdir
    self._populate_netrc(os.path.join(old_home, '.netrc'))

    if gi.cookies_path:
      cros_build_lib.RunCommand(
          ['git', 'config', '--global', 'http.cookiefile', gi.cookies_path],
          quiet=True)

    # Set netrc file for http authentication.
    self.PatchObject(gob_util, 'NETRC', netrc.netrc(gi.netrc_file))

    # Set cookie file for http authentication
    if gi.cookies_path:
      jar = cookielib.MozillaCookieJar(gi.cookies_path)
      jar.load(ignore_expires=True)

      def GetCookies(host, _path):
        ret = dict(
            (c.name, urllib.unquote(c.value)) for c in jar
            if c.domain == host and c.path == '/' and c.name in gi.cookie_names)
        return ret

      self.PatchObject(gob_util, 'GetCookies', GetCookies)

    # Make all chromite code point to the test server.
    self.PatchObject(constants, 'EXTERNAL_GOB_HOST', gi.git_host)
    self.PatchObject(constants, 'EXTERNAL_GERRIT_HOST', gi.gerrit_host)
    self.PatchObject(constants, 'EXTERNAL_GOB_URL', gi.git_url)
    self.PatchObject(constants, 'EXTERNAL_GERRIT_URL', gi.gerrit_url)
    self.PatchObject(constants, 'INTERNAL_GOB_HOST', gi.git_host)
    self.PatchObject(constants, 'INTERNAL_GERRIT_HOST', gi.gerrit_host)
    self.PatchObject(constants, 'INTERNAL_GOB_URL', gi.git_url)
    self.PatchObject(constants, 'INTERNAL_GERRIT_URL', gi.gerrit_url)
    self.PatchObject(constants, 'MANIFEST_URL', '%s/%s' % (
        gi.git_url, constants.MANIFEST_PROJECT))
    self.PatchObject(constants, 'MANIFEST_INT_URL', '%s/%s' % (
        gi.git_url, constants.MANIFEST_INT_PROJECT))
    self.PatchObject(constants, 'GIT_REMOTES', {
            constants.EXTERNAL_REMOTE: gi.gerrit_url,
            constants.INTERNAL_REMOTE: gi.gerrit_url,
            constants.CHROMIUM_REMOTE: gi.gerrit_url,
            constants.CHROME_REMOTE: gi.gerrit_url,
        })

  def createProject(self, suffix, description='Test project', owners=None,
                    submit_type='CHERRY_PICK'):
    """Create a project on the test gerrit server."""
    name = self.gerrit_instance.project_prefix + suffix
    body = {
        'description': description,
        'submit_type': submit_type,
    }
    if owners is not None:
      body['owners'] = owners
    path = 'projects/%s' % urllib.quote(name, '')
    conn = gob_util.CreateHttpConn(
        self.gerrit_instance.gerrit_host, path, reqtype='PUT', body=body)
    response = conn.getresponse()
    self.assertEquals(201, response.status,
                      'Expected 201, got %s' % response.status)
    s = cStringIO.StringIO(response.read())
    self.assertEquals(")]}'", s.readline().rstrip())
    jmsg = json.load(s)
    self.assertEquals(name, jmsg['name'])
    return name

  def _CloneProject(self, name, path):
    """Clone a project from the test gerrit server."""
    osutils.SafeMakedirs(os.path.dirname(path))
    url = '%s://%s/%s' % (
        gob_util.GIT_PROTOCOL, self.gerrit_instance.git_host, name)
    cros_build_lib.RunCommand(['git', 'clone', url, path], quiet=True)
    # Install commit-msg hook.
    hook_path = os.path.join(path, '.git', 'hooks', 'commit-msg')
    hook_cmd = ['curl', '-n', '-o', hook_path]
    if self.gerrit_instance.cookies_path:
      hook_cmd.extend(['-b', self.gerrit_instance.cookies_path])
    hook_cmd.append('https://%s/a/tools/hooks/commit-msg'
                    % self.gerrit_instance.gerrit_host)
    cros_build_lib.RunCommand(hook_cmd, quiet=True)
    os.chmod(hook_path, stat.S_IRWXU)
    # Set git identity to test account
    cros_build_lib.RunCommand(
        ['git', 'config', 'user.email', self.TEST_EMAIL], cwd=path, quiet=True)
    return path

  def cloneProject(self, name, path=None):
    """Clone a project from the test gerrit server."""
    if path is None:
      path = os.path.basename(name)
      if path.endswith('.git'):
        path = path[:-4]
    path = os.path.join(self.tempdir, path)
    return self._CloneProject(name, path)

  @classmethod
  def _CreateCommit(cls, clone_path, fn=None, msg=None, text=None, amend=False):
    """Create a commit in the given git checkout.

    Args:
      clone_path: The directory on disk of the git clone.
      fn: The name of the file to write. Optional.
      msg: The commit message. Optional.
      text: The text to append to the file. Optional.
      amend: Whether to amend an existing patch. If set, we will amend the
        HEAD commit in the checkout and upload that patch.

    Returns:
      (sha1, changeid) of the new commit.
    """
    if not fn:
      fn = 'test-file.txt'
    if not msg:
      msg = 'Test Message'
    if not text:
      text = 'Another day, another dollar.'
    fpath = os.path.join(clone_path, fn)
    osutils.WriteFile(fpath, '%s\n' % text, mode='a')
    cros_build_lib.RunCommand(['git', 'add', fn], cwd=clone_path, quiet=True)
    cmd = ['git', 'commit']
    cmd += ['--amend', '-C', 'HEAD'] if amend else ['-m', msg]
    cros_build_lib.RunCommand(cmd, cwd=clone_path, quiet=True)
    return cls._GetCommit(clone_path)

  def createCommit(self, clone_path, fn=None, msg=None, text=None,
                   amend=False):
    """Create a commit in the given git checkout.

    Args:
      clone_path: The directory on disk of the git clone.
      fn: The name of the file to write. Optional.
      msg: The commit message. Optional.
      text: The text to append to the file. Optional.
      amend: Whether to amend an existing patch. If set, we will amend the
        HEAD commit in the checkout and upload that patch.
    """
    clone_path = os.path.join(self.tempdir, clone_path)
    return self._CreateCommit(clone_path, fn, msg, text, amend)

  @staticmethod
  def _GetCommit(clone_path, ref='HEAD'):
    log_proc = cros_build_lib.RunCommand(
        ['git', 'log', '-n', '1', ref], cwd=clone_path,
        print_cmd=False, capture_output=True)
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
    return (sha1, change_id)

  def getCommit(self, clone_path, ref='HEAD'):
    """Get the sha1 and change-id for the head commit in a git checkout."""
    clone_path = os.path.join(self.tempdir, clone_path)
    (sha1, change_id) = self._GetCommit(clone_path, ref)
    self.assertTrue(sha1)
    self.assertTrue(change_id)
    return (sha1, change_id)

  @staticmethod
  def _UploadChange(clone_path, branch='master', remote='origin'):
    cros_build_lib.RunCommand(
        ['git', 'push', remote, 'HEAD:refs/for/%s' % branch], cwd=clone_path,
        quiet=True)

  def uploadChange(self, clone_path, branch='master', remote='origin'):
    """Create a gerrit CL from the HEAD of a git checkout."""
    clone_path = os.path.join(self.tempdir, clone_path)
    self._UploadChange(clone_path, branch, remote)

  @staticmethod
  def _PushBranch(clone_path, branch='master'):
    cros_build_lib.RunCommand(
        ['git', 'push', 'origin', 'HEAD:refs/heads/%s' % branch],
        cwd=clone_path, quiet=True)

  def pushBranch(self, clone_path, branch='master'):
    """Push a branch directly to gerrit, bypassing code review."""
    clone_path = os.path.join(self.tempdir, clone_path)
    self._PushBranch(clone_path, branch)

  def createAccount(self, name='Test User', email='test-user@test.org',
                    password=None, groups=None):
    """Create a new user account on gerrit."""
    username = urllib.quote(email.partition('@')[0])
    path = 'accounts/%s' % username
    body = {
        'name': name,
        'email': email,
    }

    if password:
      body['http_password'] = password
    if groups:
      if isinstance(groups, basestring):
        groups = [groups]
      body['groups'] = groups
    conn = gob_util.CreateHttpConn(
        self.gerrit_instance.gerrit_host, path, reqtype='PUT', body=body)
    response = conn.getresponse()
    self.assertEquals(201, response.status)
    s = cStringIO.StringIO(response.read())
    self.assertEquals(")]}'", s.readline().rstrip())
    jmsg = json.load(s)
    self.assertEquals(email, jmsg['email'])


class _RunCommandMock(mox.MockObject):
  """Custom mock class used to suppress arguments we don't care about"""

  DEFAULT_IGNORED_ARGS = ('print_cmd',)

  def __call__(self, *args, **kwargs):
    for arg in self.DEFAULT_IGNORED_ARGS:
      kwargs.setdefault(arg, mox.IgnoreArg())
    return mox.MockObject.__call__(self, *args, **kwargs)


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


class MoxTempDirTestCase(MoxTestCase, TempDirTestCase):
  """Convenience class mixing TempDir and Mox

  Note: mox is deprecated; please use MockTempDirTestCase instead.
  """


class MoxOutputTestCase(OutputTestCase, MoxTestCase):
  """Conevenience class mixing OutputTestCase and MoxTestCase

  Note: mox is deprecated; please use MockOutputTestCase instead.
  """


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
  results = cros_build_lib.RunCommand(
      ['find', '.', '-name', '*_unittest.py', '-printf', '%P\n'],
      cwd=directory, print_cmd=False, capture_output=True).output.splitlines()
  # Drop the trailing .py, inject in the name if one was given.
  if module_namespace:
    module_namespace += '.'
  return [module_namespace + x[:-3].replace('/', '.') for x in results]


def main(**kwargs):
  """Helper wrapper around unittest.main.  Invoke this, not unittest.main.

  Any passed in kwargs are passed directly down to unittest.main; via this, you
  can inject custom argv for example (to limit what tests run).
  """
  # Default to exit=True; this matches old behaviour, and allows unittest
  # to trigger sys.exit on its own.  Unfortunately, the exit keyword is only
  # available in 2.7- as such, handle it ourselves.
  allow_exit = kwargs.pop('exit', True)
  if '--network' in sys.argv:
    sys.argv.remove('--network')
    GlobalTestConfig.NETWORK_TESTS_DISABLED = False
  level = kwargs.pop('level', logging.CRITICAL)
  for flag in ('-d', '--debug'):
    if flag in sys.argv:
      sys.argv.remove(flag)
      level = logging.DEBUG
  cros_build_lib.SetupBasicLogging(level)
  try:
    unittest.main(**kwargs)
    raise SystemExit(0)
  except SystemExit as e:
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
