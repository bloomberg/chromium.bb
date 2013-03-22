#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cros unit test library, with utility functions."""

from __future__ import print_function
import collections
import contextlib
import cStringIO
import exceptions
import logging
import mox
import os
import re
import sys
import unittest

import osutils
import terminal
import cros_build_lib

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
      os.mkdir(f)
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

  def setUp(self):
    #pylint: disable=W0212
    osutils._TempDirSetup(self)

  def tearDown(self):
    #pylint: disable=W0212
    osutils._TempDirTearDown(self, self.sudo_cleanup)


class _RunCommandMock(mox.MockObject):
  """Custom mock class used to suppress arguments we don't care about"""

  DEFAULT_IGNORED_ARGS = ('print_cmd',)

  def __call__(self, *args, **kwds):
    for arg in self.DEFAULT_IGNORED_ARGS:
      kwds.setdefault(arg, mox.IgnoreArg())
    return mox.MockObject.__call__(self, *args, **kwds)


class LessAnnoyingMox(mox.Mox):
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
    self.mox = LessAnnoyingMox()
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


@contextlib.contextmanager
def DisableLogger(logger=None):
  """Temporarily disable logging in the specified logger.

  Arguments:
    logger: Logger to disable logging in. By default, the cros_build_lib
            logger is disabled.
  """
  if logger is None:
    logger = cros_build_lib.logger
  with mock.patch.multiple(logger, disabled=True):
    yield


def main(**kwds):
  """Helper wrapper around unittest.main.  Invoke this, not unittest.main.

  Any passed in kwds are passed directly down to unittest.main; via this, you
  can inject custom argv for example (to limit what tests run).
  """
  # Default to exit=True; this matches old behaviour, and allows unittest
  # to trigger sys.exit on its own.  Unfortunately, the exit keyword is only
  # available in 2.7- as such, handle it ourselves.
  allow_exit = kwds.pop('exit', True)
  cros_build_lib.SetupBasicLogging(kwds.pop('level', logging.DEBUG))
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
