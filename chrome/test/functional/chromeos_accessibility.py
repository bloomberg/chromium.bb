#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

import pyauto_functional
import pyauto


class AccessibilityTest(pyauto.PyUITest):
  """Tests for Accessibility. """

  def testCanEnableSpokenFeedback(self):
    """Tests that spoken feedback accessibility mode can be enabled."""

    self.EnableSpokenFeedback(True)
    self.assertTrue(self.IsSpokenFeedbackEnabled(),
                    msg='Could not enable spoken feedback accessibility mode.')


if __name__ == '__main__':
  pyauto_functional.Main()
