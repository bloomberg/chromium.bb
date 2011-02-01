#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A |unittest.TextTestRunner| that displays results like Google Test."""

import sys
import unittest


class _GTestTextTestResult(unittest._TextTestResult):
  """A test result class that can print formatted text results to a stream.

  Results printed in conformance with gtest output format, like:
  [ RUN        ] autofill.AutoFillTest.testAutofillInvalid: "test desc."
  [         OK ] autofill.AutoFillTest.testAutofillInvalid
  [ RUN        ] autofill.AutoFillTest.testFillProfile: "test desc."
  [         OK ] autofill.AutoFillTest.testFillProfile
  [ RUN        ] autofill.AutoFillTest.testFillProfileCrazyCharacters: "Test."
  [         OK ] autofill.AutoFillTest.testFillProfileCrazyCharacters
  """
  def __init__(self, stream, descriptions, verbosity):
    unittest._TextTestResult.__init__(self, stream, descriptions, verbosity)

  def _GetTestURI(self, test):
    if sys.version_info[:2] <= (2, 4):
      return '%s.%s' % (unittest._strclass(test.__class__),
                        test._TestCase__testMethodName)
    return '%s.%s' % (unittest._strclass(test.__class__), test._testMethodName)

  def getDescription(self, test):
    return '%s: "%s"' % (self._GetTestURI(test), test.shortDescription())

  def startTest(self, test):
    unittest.TestResult.startTest(self, test)
    self.stream.writeln('[ RUN        ] %s' % self.getDescription(test))

  def addSuccess(self, test):
    unittest.TestResult.addSuccess(self, test)
    self.stream.writeln('[         OK ] %s' % self._GetTestURI(test))

  def addError(self, test, err):
    unittest.TestResult.addError(self, test, err)
    self.stream.writeln('[      ERROR ] %s' % self._GetTestURI(test))

  def addFailure(self, test, err):
    unittest.TestResult.addFailure(self, test, err)
    self.stream.writeln('[     FAILED ] %s' % self._GetTestURI(test))


class GTestTextTestRunner(unittest.TextTestRunner):
  """Test runner for python unittests that displays results in textual format.

  Results are displayed in conformance with gtest output.
  """
  def __init__(self, verbosity=1):
    unittest.TextTestRunner.__init__(self,
                                     stream=sys.stderr,
                                     verbosity=verbosity)

  def _makeResult(self):
    return _GTestTextTestResult(self.stream, self.descriptions, self.verbosity)
