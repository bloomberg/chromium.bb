#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional
import pyauto


class ChromeosUpdate(pyauto.PyUITest):
  """Tests for ChromeOS updater and channel changer."""

  def testGetUpdateInfo(self):
    """Get some status info about the updater and release track."""
    result = self.GetUpdateInfo()
    self.assertTrue(result)
    logging.debug(result)


if __name__ == '__main__':
  pyauto_functional.Main()
