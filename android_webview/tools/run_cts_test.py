# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

import mock
import run_cts

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
import devil_chromium  # pylint: disable=import-error, unused-import
from devil.android.ndk import abis  # pylint: disable=import-error

class _RunCtsTest(unittest.TestCase):
  """Unittests for the run_cts module.
  """

  def testDetermineArch_arm64(self):
    logging_mock = mock.Mock()
    logging.info = logging_mock
    device = mock.Mock(product_cpu_abi=abis.ARM_64)
    self.assertEqual(run_cts.DetermineArch(device), 'arm64')
    # We should log a message to explain how we auto-determined the arch. We
    # don't assert the message itself, since that's rather strict.
    logging_mock.assert_called()

  def testDetermineArch_unsupported(self):
    device = mock.Mock(product_cpu_abi='madeup-abi')
    with self.assertRaises(Exception) as _:
      run_cts.DetermineArch(device)


if __name__ == '__main__':
  unittest.main()
