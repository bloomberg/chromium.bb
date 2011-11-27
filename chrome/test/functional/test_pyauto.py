#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import pyauto_functional  # Must be imported before pyauto
import pyauto


class PyAutoTest(pyauto.PyUITest):
  """Test functionality of the PyAuto framework."""

  _EXTRA_CHROME_FLAGS = [
    '--scooby-doo=123',
    '--donald-duck=cool',
    '--super-mario',
    '--marvin-the-martian',
  ]

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with some custom flags.

    Overrides the default list of extra flags passed to Chrome.  See
    ExtraChromeFlags() in pyauto.py.
    """
    return self._EXTRA_CHROME_FLAGS

  def testSetCustomChromeFlags(self):
    """Ensures that Chrome can be launched with custom flags."""
    self.NavigateToURL('about://version')
    for flag in self._EXTRA_CHROME_FLAGS:
      self.assertEqual(self.FindInPage(flag)['match_count'], 1,
                       msg='Missing expected Chrome flag "%s"' % flag)


if __name__ == '__main__':
  pyauto_functional.Main()
