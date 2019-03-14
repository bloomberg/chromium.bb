#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unpack_pak
import unittest


class UnpackPakTest(unittest.TestCase):
  def testMapFileLine(self):
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH}'))

  def testGzippedMapFileLine(self):
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH, false}'))
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH, true}'))

  def testUngzipString(self):
    self.assertEqual(
      unpack_pak.UngzipString(
        '\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\xff\xcbH\xcd\xc9\xc9W' +
        '(\xcf/\xcaI\x01\x00\x85\x11J\r\x0b\x00\x00\x00'),
      'hello world')


if __name__ == '__main__':
  unittest.main()
