#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for cq_cfg_presubmit."""

import mock
import os
import unittest

import cq_cfg_presubmit


class CqCfgPresubmitTest(unittest.TestCase):
  def test_verify_path_regexp_exists(self):
    with mock.patch('cq_cfg_presubmit.os.path.exists') as exists:
      exists.side_effect = [True]
      self.assertTrue(cq_cfg_presubmit.verify_path_regexps([
          'simple/file',
      ]))

  def test_verify_path_regexp_os_walk_found(self):
    with mock.patch('cq_cfg_presubmit.os.walk') as walk:
      walk.side_effect = [(
          (os.path.join(cq_cfg_presubmit.CHROMIUM_DIR, 'random'),
            None, ['test.txt'],),
          (os.path.join(cq_cfg_presubmit.CHROMIUM_DIR, 'simple/file'),
           None, ['test.txt'],),
      )]
      with mock.patch('cq_cfg_presubmit.os.path.exists') as exists:
        exists.side_effect = [False]
        self.assertTrue(cq_cfg_presubmit.verify_path_regexps([
            'simple/file/.+',
        ], False))

  def test_verify_path_regexp_os_walk_not_found(self):
    with mock.patch('cq_cfg_presubmit.os.walk') as walk:
      walk.side_effect = [(
          (os.path.join(cq_cfg_presubmit.CHROMIUM_DIR, 'random'),
            None, ['test.txt'],),
      )]
      with mock.patch('cq_cfg_presubmit.os.path.exists') as exists:
        exists.side_effect = [False]
        self.assertFalse(cq_cfg_presubmit.verify_path_regexps([
            'simple/file/.+',
        ], False))


if __name__ == '__main__':
  unittest.main()
