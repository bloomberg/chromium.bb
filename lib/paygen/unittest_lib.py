#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cros unit test library, with utility functions."""

from __future__ import print_function
import cStringIO
import mox
import os
import sys
import unittest


class ContextManagerObj(object):
  """Simulate a ContextManager object in tests.  See subclasses."""
  def __enter__(self):
    """Support context manager usage."""
    return self

  def __exit__(self, _type, _value, _traceback):
    """Support context manager usage."""


class ContextManagerObjR(ContextManagerObj):
  """Simulate a context manager object with line by line iteration.

  This can be useful when testing code like:
  with my_context_manager_obj:
    for line in my_context_manager_obj:
      do stuff

  Now in unittest code do something like:
  lines = [...]
  fake_context_manager_obj = unittest_lib.ContextManagerObjR(lines))
  for line in lines:
    test stuff
  """
  __slots__ = ('lines')

  def __init__(self, lines):
    ContextManagerObj.__init__(self)
    self.lines = lines

  def __iter__(self):
    """Declare that this class supports iteration (over lines)."""
    return self.lines.__iter__()


class FileObjR(ContextManagerObjR):
  """Simulate reading lines from a file object in a few useful scenarios.

  This can be useful when testing code like:
  with open(file_path) as file_obj:
    for line in file_obj:
      do stuff

  Now in unittest code do something like:
  self.mox.StubOutWithMock(__builtin__, 'open')
  lines = [...]
  __builtin__.open(file_path).AndReturn(unittest_lib.FileObjR(lines))
  for line in lines:
    test stuff

  TODO(mtennant): Add support for file-like read methods when needed.
  """


class FileObjW(ContextManagerObj):
  """Simulate writing lines to a file object

  This can be useful when testing code like:
  with open(path, 'w') as output:
    output.write(some_text)

  Now in unittest code do something like:
  self.mox.StubOutWithMock(__builtin__, 'open')
  output = unittest_lib.FileObjW()

  __builtin__.open(path, 'w').AndReturn(output)
  self.mox.ReplayAll()

  self.assertEquals(output.output, some_text)
  """
  __slots__ = ('output')

  def __init__(self):
    ContextManagerObj.__init__(self)
    self.output = ''

  def write(self, text):
    """Simulate file.write method, saving text."""
    self.output += text

  def writelines(self, text_lines):
    """Simulate file.writelines method, saving text."""
    self.output += ''.join(text_lines)


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


class Environ(object):
  """Class that adjusts os.environ for testing purposes.

  This class is a context manager (supports 'with') that adjusts
  os.environ with temporary values when 'active' and then resets
  it back to its original values when finished.

  It can either add to the values in os.environ or replace them
  entirely depending on the 'extend' boolean given.

  See the EnvironExtend and EnvironReplace methods of the
  TestCase class for the primary usage mechanism.
  """

  __slots__ = ('_extend', '_orig_env', '_temp_env')

  def __init__(self, extend, **kwargs):
    self._extend = extend
    self._orig_env = None
    self._temp_env = kwargs

  def IsActive(self):
    """Return True if context is currently active."""
    return self._orig_env is not None

  def Start(self):
    """Activate temporary os.environ entries."""
    if self.IsActive():
      # Already started, cannot start again.
      raise RuntimeError('Environ context manager already active.')

    self._orig_env = os.environ.copy()

    if self._extend:
      for key in self._temp_env:
        os.environ[key] = self._temp_env[key]

    else:
      os.environ = self._temp_env.copy()

  def Stop(self):
    """Replace temporary os.environ entries with originals."""
    if not self.IsActive():
      # Nothing to do if not currently active.
      return

    os.environ = self._orig_env.copy()
    os._orig_env = None

  def __enter__(self):
    """Support for entering 'with' block."""
    self.Start()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    """Support for leaving 'with' block."""
    self.Stop()


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
    """Clear any captured stdout and stderr output."""
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

    lines = output.splitlines()
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
  """Base class for cros unit tests with utility methods."""

  __slots__ = ('_output_capturer', '_environ')

  def __init__(self, arg):
    """Base class __init__ takes a second argument."""
    unittest.TestCase.__init__(self, arg)
    self._output_capturer = None
    self._environ = None

  def EnvironExtend(self, **kwargs):
    """Extend os.environ temporarily with keys/values in kwargs."""
    self._environ = Environ(True, **kwargs)
    return self._environ

  def EnvironReplace(self, **kwargs):
    """Replace os.environ temporarily with keys/values in kwargs."""
    self._environ = Environ(False, **kwargs)
    return self._environ

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

  def _GenCheckMsgFunc(self, line_re):
    """Return boolean func to check a line with regex |line_re|.

    Args:
      line_re: Regex that function will use on all lines.

    Returns:
      Function that returns true if input line matches line_re.
    """
    # pylint: disable-msg=C0111,W0612
    def _method(line):
      return line_re.search(line)

    # Provide a description of what this function looks for in a line.  Error
    # messages can make use of this.
    _method.description = 'line matching regexp %r' % line_re.pattern
    return _method

  def _ContainsMsgLine(self, lines, msg_check_func):
    """Return True if msg_check_func returns true for any line in lines."""
    return any(msg_check_func(ln) for ln in lines)

  def _GenOutputDescription(self, check_stdout, check_stderr):
    """Some extra logic to make an error message useful."""
    if check_stdout and check_stderr:
      return 'stdout or stderr'
    elif check_stdout:
      return 'stdout'
    elif check_stderr:
      return 'stderr'

  def _AssertOutputContainsMsg(self, check_msg_func, invert,
                               check_stdout, check_stderr):
    """Assert that output contains or does not contain msg."""
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

  def AssertOutputContainsLine(self, regexp, invert=False,
                               check_stdout=True, check_stderr=False):
    """Assert requested output contains line matching |regexp|.

    If |invert| is true, then assert the line is NOT found.

    Args:
      regexp: Regexp to check against any line for a match.
      invert: If True then assertion is that output does not
        contain a matching line.
      check_stdout: If True, look through lines of stdout
      check_stderr: If True, look through lines of stderr

    Raises:
      RuntimeError if output capturing was never one for this test.
    """
    check_msg_func = self._GenCheckMsgFunc(regexp)
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

  def AssertOutputEndsInLine(self, regexp,
                             check_stdout=True, check_stderr=False):
    """Assert requested output ends in line matching |regexp|.

    Args:
      regexp: Regexp to check against any line for a match.
      check_stdout: If True, look at last line of stdout
      check_stderr: If True, look at last line of stderr

    Raises:
      RuntimeError if output capturing was never one for this test.
    """
    check_msg_func = self._GenCheckMsgFunc(regexp)
    return self._AssertOutputEndsInMsg(check_msg_func,
                                       check_stdout, check_stderr)

  def AssertFuncSystemExit(self, func, *args, **kwargs):
    """Run |func| with |args| and |kwargs| and assert SystemExit.

    Assert that func raised SystemExit, and return exit code.

    Args:
      func: Function to run, catching SystemExit
      args: *args to pass to func
      kwargs: **kwargs to pass to func

    Returns:
      Exit code of caught SystemExit, if assertion passes.
    """
    try:
      func(*args, **kwargs)
    except SystemExit as e:
      return e.code

    self.assertFalse(True, 'Expected to catch a SystemExit.')

  def AssertFuncSystemExitZero(self, func, *args, **kwargs):
    """Run |func| with |args| and |kwargs| catching SystemExit.

    If the func does not raise a SystemExit with exit code 0 then assert.
    Note that an exit code of None or False is also acceptable, following
    the behavior os sys.exit() function.

    Args:
      func: Function to run as func(*args, **kwargs)
      args: *args to pass to func
      kwargs: **kwargs to pass to func
    """
    exit_code = self.AssertFuncSystemExit(func, *args, **kwargs)
    self.assertTrue(exit_code == 0 or exit_code is None or exit_code is False,
                    msg='Expected system exit code 0, but caught %s' %
                    exit_code)

  def AssertFuncSystemExitNonZero(self, func, *args, **kwargs):
    """Run |func| with |args| and |kwargs| catching SystemExit.

    If the func does not raise a non-zero SystemExit code then assert.

    Args:
      func: Function to run as func(*args, **kwargs)
      args: *args to pass to func
      kwargs: **kwargs to pass to func
    """
    exit_code = self.AssertFuncSystemExit(func, *args, **kwargs)
    self.assertTrue(exit_code,
                    msg='Expected non-zero system exit code, but caught %s' %
                    exit_code)

  def AssertRaisesAndReturn(self, error, func, *args, **kwargs):
    """Like assertRaises, but return exception raised.

    Enhance the assertRaises class to also return the exception that was raised.

    Args:
      error: The expected exception
      func: Function to run as func(*args, **kwargs)
      args: *args to pass to func
      kwargs: **kwargs to pass to func

    Returns:
      The exception raised
    """
    try:
      func(*args, **kwargs)
      self.assertTrue(False, msg='Expected %s but got none' % error)
    except error as ex:
      return ex


class MoxTestCase(TestCase, mox.MoxTestBase):
  """Add mox.MoxTestBase super class to TestCase."""
