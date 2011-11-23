#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from test_expectations import TestExpectations


class TestTestExpectations(unittest.TestCase):

  def testParseLine(self):
    line = "BUGCR86714 MAC GPU : media/video-zoom.html = CRASH IMAGE"
    comments = 'Comments'
    expected_map = {'CRASH': True, 'IMAGE': True, 'Bugs': ['BUGCR86714'],
                    'Comments': 'Comments', 'MAC': True, 'GPU': True}
    self.assertEquals(TestExpectations.ParseLine(line, comments),
                      expected_map)

  def testParseLineWithLineComments(self):
    line = "BUGCR86714 MAC GPU : media/video-zoom.html = CRASH IMAGE // foo"
    comments = 'Comments'
    expected_map = {'CRASH': True, 'IMAGE': True, 'Bugs': ['BUGCR86714'],
                    'Comments': 'Comments foo', 'MAC': True, 'GPU': True}
    self.assertEquals(TestExpectations.ParseLine(line, comments),
                      expected_map)

  def testParseLineWithLineGPUComments(self):
    line = "BUGCR86714 MAC : media/video-zoom.html = CRASH IMAGE // GPU"
    comments = 'Comments'
    expected_map = {'CRASH': True, 'IMAGE': True, 'Bugs': ['BUGCR86714'],
                    'Comments': 'Comments GPU', 'MAC': True}
    self.assertEquals(TestExpectations.ParseLine(line, comments),
                      expected_map)

  def testExtractTestOrDirectoryName(self):
    line = ('BUGWK58013 MAC GPU : compositing/scaling/'
            'tiled-layer-recursion.html = CRASH')
    self.assertEquals(TestExpectations.ExtractTestOrDirectoryName(line),
                      'compositing/scaling/tiled-layer-recursion.html')

  def testExtractTestOrDirectoryNameWithSvg(self):
    line = 'BUGWK43668 SKIP : media/track/x.svg = TIMEOUT'
    self.assertEquals(TestExpectations.ExtractTestOrDirectoryName(line),
                      'media/track/x.svg')

  def testExtractTestOrDirectoryNameWithDirName(self):
    line = 'BUGWK43668 SKIP : media/track/ = TIMEOUT'
    self.assertEquals(TestExpectations.ExtractTestOrDirectoryName(line),
                      'media/track/')

  def testExtractTestOrDirectoryNameWithException(self):
    self.assertRaises(ValueError,
                      TestExpectations.ExtractTestOrDirectoryName, 'Foo')


if __name__ == '__main__':
  unittest.main()
