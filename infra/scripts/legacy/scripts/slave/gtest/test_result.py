# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def canonical_name(name):
  new_name = name.replace('FLAKY_', '', 1)
  new_name = new_name.replace('FAILS_', '', 1)
  new_name = new_name.replace('DISABLED_', '', 1)
  return new_name


class TestResult(object):
  """A simple class that represents a single test result."""

  # Test modifier constants.
  (NONE, FAILS, FLAKY, DISABLED) = range(4)

  def __init__(self, test, failed, not_run=False, elapsed_time=0):
    self.test_name = canonical_name(test)
    self.failed = failed
    self.test_run_time = elapsed_time

    test_name = test
    try:
      test_name = test.split('.')[1]
    except IndexError:
      pass

    if not_run:
      self.modifier = self.DISABLED
    elif test_name.startswith('FAILS_'):
      self.modifier = self.FAILS
    elif test_name.startswith('FLAKY_'):
      self.modifier = self.FLAKY
    elif test_name.startswith('DISABLED_'):
      self.modifier = self.DISABLED
    else:
      self.modifier = self.NONE

  def fixable(self):
    return self.failed or self.modifier == self.DISABLED
