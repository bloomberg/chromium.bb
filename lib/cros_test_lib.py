#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cros unit test library, with utility functions."""

from __future__ import print_function
import cStringIO
import exceptions
import mox
import re
import sys
import unittest

import osutils
import terminal


class TempDirMixin(object):
  """Mixin used to give each test a tempdir that is cleansed upon finish"""

  def setUp(self):
    osutils._TempDirSetup(self)

  def tearDown(self):
    osutils._TempDirTearDown(self)


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
  """Base class for cros unit tests with utility methods."""

  __slots__ = ['_output_capturer']

  def __init__(self, arg):
    """Base class __init__ takes a second argument."""
    unittest.TestCase.__init__(self, arg)
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
    returnval, exit_code = self.FuncCatchSystemExit(func, *args, **kwargs)
    self.assertFalse(exit_code is None,
                      msg='Expected system exit code 0, but caught none')
    self.assertTrue(exit_code == 0,
                    msg='Expected system exit code 0, but caught %d' %
                    exit_code)

  def AssertFuncSystemExitNonZero(self, func, *args, **kwargs):
    """Run |func| with |args| and |kwargs| catching exceptions.SystemExit.

    If the func does not raise a non-zero SystemExit code then assert.
    """
    returnval, exit_code = self.FuncCatchSystemExit(func, *args, **kwargs)
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

class MoxTestCase(TestCase, mox.MoxTestBase):
  """Add mox.MoxTestBase super class to TestCase."""
