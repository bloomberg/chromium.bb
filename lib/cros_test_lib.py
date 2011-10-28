#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cros unit test library, with utility functions."""

import cStringIO
import re
import sys
import unittest

import mox

# pylint: disable=W0212,R0904

class TestCase(unittest.TestCase):
  """Base class for cros unit tests with utility methods."""

  # This works with error output from operation module.
  ERROR_PREFIX = re.compile('^\033\[1;31m')

  __slots__ = ['_stderr', '_stderr_cap', '_stdout', '_stdout_cap']

  def __init__(self, arg):
    """Base class __init__ takes a second argument."""
    unittest.TestCase.__init__(self, arg)

    self._stdout = None
    self._stderr = None
    self._stdout_cap = None
    self._stderr_cap = None

  def _IsErrorLine(self, line):
    """Return True if |line| has prefix associated with error output."""
    return self.ERROR_PREFIX.search(line)

  def _StartCapturingOutput(self):
    """Begin capturing stdout and stderr."""
    self._stdout = sys.stdout
    self._stderr = sys.stderr
    sys.stdout = self._stdout_cap = cStringIO.StringIO()
    sys.stderr = self._stderr_cap = cStringIO.StringIO()

  def _RetrieveCapturedOutput(self):
    """Return captured output so far as (stdout, stderr) tuple."""
    try:
      if self._stdout and self._stderr:
        return (self._stdout_cap.getvalue(), self._stderr_cap.getvalue())
      return None
    except AttributeError:
      # This will happen if output capturing was never on.
      return None

  def _StopCapturingOutput(self):
    """Stop capturing stdout and stderr."""
    try:
      sys.stdout = self._stdout
      sys.stderr = self._stderr
      self._stdout = None
      self._stderr = None
    except AttributeError:
      # This will happen if output capturing was never on.
      pass

  def _AssertOutputEndsInError(self, output):
    """Return True if |output| ends with an error message."""
    lastline = [ln for ln in output.split('\n') if ln][-1]
    self.assertTrue(self._IsErrorLine(lastline),
                    msg="expected output to end in error line, but "
                    "_IsErrorLine says this line is not an error:\n%s" %
                    lastline)

class MoxTestCase(TestCase, mox.MoxTestBase):
  """Add mox.MoxTestBase super class to TestCase."""
