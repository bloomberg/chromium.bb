#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from path_util import SplitParent


class PathUtilTest(unittest.TestCase):

  def testSplitParent(self):
    self.assertEqual(('', 'hi'), SplitParent('hi'))
    self.assertEqual(('', 'hi/'), SplitParent('hi/'))
    self.assertEqual(('/', 'hi'), SplitParent('/hi'))
    self.assertEqual(('/', 'hi/'), SplitParent('/hi/'))
    self.assertEqual(('parent', 'hi'), SplitParent('parent/hi'))
    self.assertEqual(('parent', 'hi/'), SplitParent('parent/hi/'))
    self.assertEqual(('/parent', 'hi'), SplitParent('/parent/hi'))
    self.assertEqual(('/parent', 'hi/'), SplitParent('/parent/hi/'))
    self.assertEqual(('p1/p2', 'hi'), SplitParent('p1/p2/hi'))
    self.assertEqual(('p1/p2', 'hi/'), SplitParent('p1/p2/hi/'))
    self.assertEqual(('/p1/p2', 'hi'), SplitParent('/p1/p2/hi'))
    self.assertEqual(('/p1/p2', 'hi/'), SplitParent('/p1/p2/hi/'))


if __name__ == '__main__':
  unittest.main()
