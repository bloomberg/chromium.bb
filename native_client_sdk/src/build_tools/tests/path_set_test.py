#!/usr/bin/python2.6
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for path_set.py."""

import os
import sys
import unittest

from build_tools import path_set


class TestPathSet(unittest.TestCase):
  """This class tests basic functionality of the installer_contents package"""

  def setUp(self):
    self.pathset = path_set.PathSet()

  def testConstructor(self):
    """Test default constructor."""
    self.assertEqual(0, len(self.pathset.files))
    self.assertEqual(0, len(self.pathset.dirs))
    self.assertEqual(0, len(self.pathset.symlinks))
    self.assertEqual(0, len(self.pathset.links))

  def testSetAttributes(self):
    self.pathset.files = set(['file1', 'file2'])
    self.pathset.dirs = set(['dir1', 'dir2'])
    self.pathset.symlinks = {'symlink': 'symlink_target'}
    self.pathset.links = {'link': 'link_target'}
    self.assertEqual(2, len(self.pathset.files))
    self.assertTrue('file1' in self.pathset.files)
    self.assertTrue('file2' in self.pathset.files)
    self.assertFalse('dir1' in self.pathset.files)
    self.assertEqual(2, len(self.pathset.dirs))
    self.assertTrue('dir1' in self.pathset.dirs)
    self.assertTrue('dir2' in self.pathset.dirs)
    self.assertFalse('file1' in self.pathset.dirs)
    self.assertEqual(1, len(self.pathset.symlinks))
    self.assertTrue('symlink' in self.pathset.symlinks)
    self.assertFalse('dir1' in self.pathset.symlinks)
    self.assertEqual(1, len(self.pathset.links))
    self.assertTrue('link' in self.pathset.links)
    self.assertFalse('file1' in self.pathset.links)
    self.pathset.dirs.discard('dir1')
    self.assertFalse('dir11' in self.pathset.dirs)

  def testSetBadAttributes(self):
    try:
      self.pathset.files = ['this', 'is', 'a', 'list']
      self.fail('set files failed to throw an exception with non-set')
    except path_set.Error:
      pass
    else:
      raise

    try:
      self.pathset.dirs = ['dir', 'list']
      self.fail('set dirs failed to throw an exception with non-set')
    except path_set.Error:
      pass
    else:
      raise

    try:
      self.pathset.symlinks = 10
      self.fail('set symlinks failed to throw an exception with non-dict')
    except path_set.Error:
      pass
    else:
      raise

    try:
      self.pathset.links = 'string of links'
      self.fail('set links failed to throw an exception with non-dict')
    except path_set.Error:
      pass
    else:
      raise

  def testOrOperator(self):
    self.pathset.files = set(['file1', 'file2', 'file3'])
    self.pathset.dirs = set(['dir1', 'dir2'])
    self.pathset.symlinks = {'symlink': 'symlink_target'}
    self.pathset.links = {'link': 'link_target'}
    pathset2 = path_set.PathSet()
    pathset2.files = set(['file1', 'file4'])
    pathset2.dirs = set(['dir1', 'dir3'])
    pathset2.symlinks = {'symlink2': 'symlink_target2',
                         'dir2': 'link_to_dir',
                         'file3': 'link_to_file'}
    pathset2.links = {'link2': 'link_target2'}
    merged_pathset = self.pathset | pathset2
    self.assertFalse('file3' in self.pathset.files)
    self.assertFalse('dir3' in self.pathset.dirs)
    self.assertTrue('file1' in merged_pathset.files)
    self.assertTrue('file2' in merged_pathset.files)
    self.assertFalse('file3' in merged_pathset.files)
    self.assertTrue('file4' in merged_pathset.files)
    self.assertFalse('dir1' in merged_pathset.files)
    self.assertTrue('dir1' in merged_pathset.dirs)
    self.assertFalse('dir2' in merged_pathset.dirs)
    self.assertTrue('dir3' in merged_pathset.dirs)
    self.assertFalse('file1' in merged_pathset.dirs)
    self.assertTrue('symlink' in merged_pathset.symlinks)
    self.assertTrue('symlink2' in merged_pathset.symlinks)
    self.assertFalse('dir1' in merged_pathset.symlinks)
    self.assertTrue('link' in merged_pathset.links)
    self.assertTrue('link2' in merged_pathset.links)
    self.assertFalse('file1' in merged_pathset.links)

  def testOrEqualsOperator(self):
    self.pathset.files = set(['file1', 'file2', 'file3'])
    self.pathset.dirs = set(['dir1', 'dir2'])
    self.pathset.symlinks = {'symlink': 'symlink_target'}
    self.pathset.links = {'link': 'link_target'}
    pathset2 = path_set.PathSet()
    pathset2.files = set(['file1', 'file4'])
    pathset2.dirs = set(['dir1', 'dir3'])
    pathset2.symlinks = {'symlink2': 'symlink_target2',
                         'dir2': 'link_to_dir',
                         'file3': 'link_to_file'}
    pathset2.links = {'link2': 'link_target2'}
    self.pathset |= pathset2
    self.assertTrue('file1' in self.pathset.files)
    self.assertTrue('file2' in self.pathset.files)
    self.assertFalse('file3' in self.pathset.files)
    self.assertFalse('dir1' in self.pathset.files)
    self.assertTrue('dir1' in self.pathset.dirs)
    self.assertFalse('dir2' in self.pathset.dirs)
    self.assertTrue('dir3' in self.pathset.dirs)
    self.assertFalse('file1' in self.pathset.dirs)
    self.assertTrue('symlink' in self.pathset.symlinks)
    self.assertTrue('symlink2' in self.pathset.symlinks)
    self.assertFalse('dir1' in self.pathset.symlinks)
    self.assertTrue('link' in self.pathset.links)
    self.assertTrue('link2' in self.pathset.links)
    self.assertFalse('file1' in self.pathset.links)

  def testPrependPath(self):
    path_prefix = os.path.join('C:%s' % os.sep, 'path', 'prefix')
    self.pathset.files = set(['file1', 'file2'])
    self.pathset.dirs = set(['dir1', 'dir2'])
    self.pathset.symlinks = {'symlink': 'symlink_target'}
    self.pathset.links = {'link': 'link_target'}
    prepended_files = set([os.path.join(path_prefix, f)
                           for f in self.pathset.files])
    prepended_dirs = set([os.path.join(path_prefix, d)
                          for d in self.pathset.dirs])
    prepended_symlinks = {os.path.join(path_prefix, 'symlink'):
                              'symlink_target'}
    prepended_links = {os.path.join(path_prefix, 'link'): 'link_target'}
    self.pathset.PrependPath(path_prefix)
    self.assertTrue(isinstance(self.pathset.files, set))
    self.assertTrue(isinstance(self.pathset.dirs, set))
    self.assertTrue(isinstance(self.pathset.symlinks, dict))
    self.assertTrue(isinstance(self.pathset.links, dict))
    self.assertEqual(prepended_files, self.pathset.files)
    self.assertEqual(prepended_dirs, self.pathset.dirs)
    self.assertEqual(prepended_symlinks, self.pathset.symlinks)
    self.assertEqual(prepended_links, self.pathset.links)


def RunTests():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestPathSet)
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(RunTests())
