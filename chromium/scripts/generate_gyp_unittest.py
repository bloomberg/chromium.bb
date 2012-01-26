#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for generate_gyp_py.

It's tough to test the lower-level GetSourceFiles() and GetObjectFiles()
functions, so this focuses on the higher-level functions assuming those two
functions are working as intended (i.e., producing lists of files).
"""

import string 
import unittest
import generate_gyp as gg

class ModuleUnittest(unittest.TestCase):
  def testGetObjectToSourceMapping(self):
    srcs = [
        'a.c',
        'b.asm',
        'c.cc',
    ]
    expected = {
        'a.o': 'a.c',
        'b.o': 'b.asm',
        'c.o': 'c.cc',
    }
    self.assertEqual(expected, gg.GetObjectToSourceMapping(srcs))

  def testGetSourceFileSet(self):
    objs_to_srcs = {
        'a.o': 'a.c',
        'b.o': 'b.asm',
        'c.o': 'c.cc',
    }
    objs = [
        'a.o',
        'c.o',
    ]
    expected = set(['a.c', 'c.cc'])
    self.assertEqual(expected, gg.GetSourceFileSet(objs_to_srcs, objs))

  def testGetSourceFileSet_NotFound(self):
    objs_to_srcs = {
        'a.o': 'a.c',
        'b.o': 'b.asm',
        'c.o': 'c.cc',
    }
    objs = [
        'd.o',
    ]
    self.assertRaises(KeyError, gg.GetSourceFileSet, objs_to_srcs, objs)

class SourceSetUnittest(unittest.TestCase):
  def testEquals(self):
    a = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))
    b = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))
    c = gg.SourceSet(set(['c', 'd']), set(['1']), set(['2']))
    d = gg.SourceSet(set(['a', 'b']), set(['0']), set(['2']))
    e = gg.SourceSet(set(['a', 'b']), set(['1']), set(['0']))

    self.assertEqual(a, b)
    self.assertNotEqual(a, c)
    self.assertNotEqual(a, d)
    self.assertNotEqual(a, e)

  def testIntersect_Exact(self):
    a = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))
    b = gg.SourceSet(set(['a', 'b']), set(['3']), set(['4']))

    c = a.Intersect(b)

    self.assertEqual(c.sources, set(['a', 'b']))
    self.assertEqual(c.architectures, set(['1', '3']))
    self.assertEqual(c.targets, set(['2', '4']))
    self.assertFalse(c.IsEmpty())

  def testIntersect_Disjoint(self):
    a = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))
    b = gg.SourceSet(set(['c', 'd']), set(['3']), set(['4']))

    c = a.Intersect(b)

    self.assertEqual(c.sources, set())
    self.assertEqual(c.architectures, set(['1', '3']))
    self.assertEqual(c.targets, set(['2', '4']))
    self.assertTrue(c.IsEmpty())

  def testIntersect_Overlap(self):
    a = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))
    b = gg.SourceSet(set(['b', 'c']), set(['3']), set(['4']))

    c = a.Intersect(b)

    self.assertEqual(c.sources, set(['b']))
    self.assertEqual(c.architectures, set(['1', '3']))
    self.assertEqual(c.targets, set(['2', '4']))
    self.assertFalse(c.IsEmpty())

  def testDifference_Exact(self):
    a = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))
    b = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))

    c = a.Difference(b)

    self.assertEqual(c.sources, set())
    self.assertEqual(c.architectures, set(['1']))
    self.assertEqual(c.targets, set(['2']))
    self.assertTrue(c.IsEmpty())

  def testDifference_Disjoint(self):
    a = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))
    b = gg.SourceSet(set(['c', 'd']), set(['3']), set(['4']))

    c = a.Difference(b)

    self.assertEqual(c.sources, set(['a', 'b']))
    self.assertEqual(c.architectures, set())
    self.assertEqual(c.targets, set())
    self.assertTrue(c.IsEmpty())

  def testDifference_Overlap(self):
    a = gg.SourceSet(set(['a', 'b']), set(['1']), set(['2']))
    b = gg.SourceSet(set(['b', 'c', 'd']), set(['1', '3']), set(['2', '4']))

    c = a.Difference(b)

    self.assertEqual(c.sources, set(['a']))
    self.assertEqual(c.architectures, set(['1']))
    self.assertEqual(c.targets, set(['2']))
    self.assertFalse(c.IsEmpty())

  def testGenerateGypStanza(self):
    # ia32 should just be ia32.
    a = gg.SourceSet(set(['a', 'b']), set(['ia32']), set(['Chromium']))
    string.index(a.GenerateGypStanza(),
                 'target_arch=="ia32"')

    # x64 should just be x64.
    a = gg.SourceSet(set(['a', 'b']), set(['x64']), set(['Chromium']))
    string.index(a.GenerateGypStanza(),
                 'target_arch=="x64"')

    # arm should just be arm.
    a = gg.SourceSet(set(['a', 'b']), set(['arm']), set(['Chromium']))
    string.index(a.GenerateGypStanza(),
                 'target_arch=="arm"')

    # arm-neon should be arm and flip the arm_neon switch.
    a = gg.SourceSet(set(['a', 'b']), set(['arm-neon']), set(['Chromium']))
    string.index(a.GenerateGypStanza(),
                 'target_arch=="arm" and arm_neon==1')

    # All architectures case.
    a = gg.SourceSet(set(['a', 'b']), set(['arm', 'arm-neon', 'x64', 'ia32']),
                     set(['Chromium']))
    string.index(a.GenerateGypStanza(), '(1)')

    # All targets case.
    a = gg.SourceSet(set(['a', 'b']), set(['arm']),
                     set(['Chromium', 'ChromiumOS', 'Chrome', 'ChromeOS']))
    string.index(a.GenerateGypStanza(), '(1)')

  def assertEqualSets(self, actual, expected):
    # Do pairwise checks for easier debugging.
    for a in actual:
      self.assertTrue(a in expected, msg='Unexpected set: %s' % a)
    for e in expected:
      self.assertTrue(e in actual, msg='Did not find expected set: %s' % e)

  def testCreatePairwiseDisjointSets_Pair(self):
    a = gg.SourceSet(set(['common', 'intel']), set(['ia32']),
                     set(['Chromium']))
    b = gg.SourceSet(set(['common', 'intel', 'chrome']), set(['ia32']),
                     set(['Chrome']))

    expected = []
    expected.append(gg.SourceSet(set(['common', 'intel']), set(['ia32']),
                                 set(['Chromium', 'Chrome'])))
    expected.append(gg.SourceSet(set(['chrome']), set(['ia32']),
                                 set(['Chrome'])))

    sets = gg.CreatePairwiseDisjointSets([a, b])
    self.assertEqualSets(sets, expected)

  def testCreatePairwiseDisjointSets_Triplet(self):
    a = gg.SourceSet(set(['common', 'intel']), set(['ia32']), set(['Chromium']))
    b = gg.SourceSet(set(['common', 'intel', 'chrome']), set(['x64']),
                     set(['Chrome']))
    c = gg.SourceSet(set(['common', 'arm']), set(['arm']), set(['Chromium']))

    expected = []
    expected.append(gg.SourceSet(set(['common']), set(['ia32', 'x64', 'arm']),
                                 set(['Chromium', 'Chrome'])))
    expected.append(gg.SourceSet(set(['intel']), set(['ia32', 'x64']),
                                 set(['Chromium', 'Chrome'])))
    expected.append(gg.SourceSet(set(['chrome']), set(['x64']),
                                 set(['Chrome'])))
    expected.append(gg.SourceSet(set(['arm']), set(['arm']), set(['Chromium'])))

    sets = gg.CreatePairwiseDisjointSets([a, b, c])
    self.assertEqualSets(sets, expected)

  def testCreatePairwiseDisjointSets_Multiple(self):
    a = gg.SourceSet(set(['common', 'intel']),
                     set(['ia32']),
                     set(['Chromium']))
    b = gg.SourceSet(set(['common', 'intel', 'chrome']),
                     set(['ia32']),
                     set(['Chrome']))
    c = gg.SourceSet(set(['common', 'intel']),
                     set(['x64']),
                     set(['Chromium']))
    d = gg.SourceSet(set(['common', 'intel', 'chrome']),
                     set(['x64']),
                     set(['Chrome']))
    e = gg.SourceSet(set(['common', 'arm']),
                     set(['arm']),
                     set(['Chromium']))
    f = gg.SourceSet(set(['common', 'arm-neon', 'chrome', 'chromeos']),
                     set(['arm-neon']),
                     set(['ChromeOS']))

    expected = []
    expected.append(gg.SourceSet(set(['common']),
                                 set(['ia32', 'x64', 'arm', 'arm-neon']),
                                 set(['Chromium', 'Chrome', 'ChromeOS'])))
    expected.append(gg.SourceSet(set(['intel']),
                                 set(['ia32', 'x64']),
                                 set(['Chromium', 'Chrome'])))
    expected.append(gg.SourceSet(set(['arm']),
                                 set(['arm']),
                                 set(['Chromium'])))
    expected.append(gg.SourceSet(set(['chrome']),
                                 set(['ia32', 'x64', 'arm-neon']),
                                 set(['Chrome', 'ChromeOS'])))
    expected.append(gg.SourceSet(set(['arm-neon', 'chromeos']),
                                 set(['arm-neon']),
                                 set(['ChromeOS'])))

    sets = gg.CreatePairwiseDisjointSets([a, b, c, d, e, f])
    self.assertEqualSets(sets, expected)

if __name__ == '__main__':
  unittest.main()
