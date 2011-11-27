#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for apply_patch.py."""

import os
import sys
import unittest
import tempfile

from build_tools import apply_patch


class TestRange(unittest.TestCase):
  """ Test class _Range. """

  def setUp(self):
    pass

  def testParseValidRange(self):
    """ Test _Range.Parse with a valid range. """
    diff_line = ['@@ -1,10 +5,9 @@']
    range = apply_patch._Range()
    range.Parse(diff_line)
    self.assertEqual(range.src_start_line, 1)
    self.assertEqual(range.src_line_count, 10)
    self.assertEqual(range.dest_start_line, 5)
    self.assertEqual(range.dest_line_count, 9)

  def testParseValidRangeWithDefaults(self):
    """ Test _Range.Parse with a valid range with default counts. """
    diff_line = ['@@ -1 +5 @@']
    range = apply_patch._Range()
    range.Parse(diff_line)
    self.assertEqual(range.src_start_line, 1)
    self.assertEqual(range.src_line_count, 1)
    self.assertEqual(range.dest_start_line, 5)
    self.assertEqual(range.dest_line_count, 1)

  def testParseInvalidRanges(self):
    """ Test _Range.Parse with invalid ranges. """
    diff_line = ['@@ ,10 +5,9 @@']
    range = apply_patch._Range()
    self.assertRaises(apply_patch.Error, range.Parse, diff_line)
    diff_line = ['@@ -,10 +5,9 @@']
    range = apply_patch._Range()
    self.assertRaises(apply_patch.Error, range.Parse, diff_line)
    diff_line = ['@@ -1,10 5,9 @@']
    range = apply_patch._Range()
    self.assertRaises(apply_patch.Error, range.Parse, diff_line)
    diff_line = ['@ -5,10 +5,9 @@']
    range = apply_patch._Range()
    self.assertRaises(apply_patch.Error, range.Parse, diff_line)


class TestChangeHunk(unittest.TestCase):
  """ Test class _ChangeHunk. """

  def setUp(self):
    self._change_hunk = apply_patch._ChangeHunk()

  def testDiffLineType(self):
    """ Test function _ChangeHunk._DiffLineType. """
    diff_line_pairs = [
        [' a contextual line', apply_patch.CONTEXTUAL_DIFF_LINE],
        ['\n', apply_patch.CONTEXTUAL_DIFF_LINE],
        ['\r', apply_patch.CONTEXTUAL_DIFF_LINE],
        [ '\n\r', apply_patch.CONTEXTUAL_DIFF_LINE],
        ['+one added line', apply_patch.ADDED_DIFF_LINE],
        ['-one delete line', apply_patch.DELETED_DIFF_LINE],
        ['not valid', apply_patch.NOT_A_DIFF_LINE],
        ['---not valid', apply_patch.NOT_A_DIFF_LINE],
        ['+++not valid', apply_patch.NOT_A_DIFF_LINE],
      ]
    for pair in diff_line_pairs:
      self.assertEqual(self._change_hunk._DiffLineType(pair[0]), pair[1])

  def testParseWithValidLines(self):
    """ Test function _ChangeHunk.Parse with valid diff lines. """
    diff_lines = [
        '@@ -5,4 +10,4 @@',
        ' A contextual line.',
        '-A line deleted from source.',
        '-Another deleted line.',
        '\r',  # An empty contextual line.
        '+A line added to destination.',
        '+Another added line.',
        '--- Begin next header.',
      ]
    num_input_lines = len(diff_lines)
    self._change_hunk.Parse(diff_lines)

    self.assertEqual(self._change_hunk.range.src_start_line, 5)
    self.assertEqual(self._change_hunk.range.src_line_count, 4)
    self.assertEqual(self._change_hunk.range.dest_start_line, 10)
    self.assertEqual(self._change_hunk.range.dest_line_count, 4)
    self.assertEqual(len(self._change_hunk.lines), num_input_lines - 2)

  def testParseWithInvalidLines(self):
    """ Test function _ChangeHunk.Parse with invalid diff lines. """
    diff_lines = [
        '@@ -5,4 +10,4 @@',
        ' A contextual line.',
        '-Another deleted line.',
        '\r',  # An empty contextual line.
        '+A line added to destination.',
        '+Another added line.',
        '--- Begin next header.',
      ]
    num_input_lines = len(diff_lines)
    self.assertRaises(apply_patch.Error, self._change_hunk.Parse, diff_lines)

  def testApply(self):
    """ Test function _ChangeHunk.Apply. """
    with tempfile.SpooledTemporaryFile() as in_file:
      with tempfile.SpooledTemporaryFile() as out_file:
        in_file.write('aaaaaaaaaa\n')
        in_file.write('bbbbbbbbbb\n')
        in_file.seek(0)

        diff_lines = [
            '@@ -1,2 +1,2 @@',
            ' aaaaaaaaaa\n',
            '-bbbbbbbbbb\n',
            '+cccccccccc\n',
            '--- Begin next header.\n',
          ]

        self._change_hunk.Parse(diff_lines)
        self._change_hunk.Apply(0, in_file, out_file)
        out_file.seek(0)
        self.assertEqual(out_file.readline(), 'aaaaaaaaaa\n');
        self.assertEqual(out_file.readline(), 'cccccccccc\n');

class TestPatchHeader(unittest.TestCase):
  """ Test class _PatchHeader. """

  def setUp(self):
    self._header = apply_patch._PatchHeader()

  def testParseValidHeader(self):
    """ Test function _PatchHeader.Parse with a valid header. """
    diff_lines = [
        '--- dir/file.txt 1969-12-31 17:00:00.000000000 -0700\n',
        '+++ dir/file_new.txt 2010-07-08 09:49:37.000000000 -0600\n'
      ]
    self._header.Parse(diff_lines)
    self.assertEqual(self._header.in_file_name, 'dir/file.txt')
    self.assertEqual(self._header.out_file_name, 'dir/file_new.txt')

  def testParseInvalidHeader(self):
    """ Test function _PatchHeader.Parse with invalid headers. """
    diff_lines = [
        '-- dir/file.txt  1969-12-31 17:00:00.000000000 -0700\n',
        '+++ dir/file_new.txt 2010-07-08 09:49:37.000000000 -0600\n'
      ]
    self.assertRaises(apply_patch.Error, self._header.Parse, diff_lines)
    diff_lines = [
        '---\n',
        '+++ dir/file_new.txt 2010-07-08 09:49:37.000000000 -0600\n'
      ]
    self.assertRaises(apply_patch.Error, self._header.Parse, diff_lines)
    diff_lines = [
        '-- dir/file.txt  1969-12-31 17:00:00.000000000 -0700\n',
        '+++\n'
      ]
    self.assertRaises(apply_patch.Error, self._header.Parse, diff_lines)
    diff_lines = [
        '@@ dir/file.txt  1969-12-31 17:00:00.000000000 -0700\n',
        '+++ dir/file_new.txt 2010-07-08 09:49:37.000000000 -0600\n'
      ]
    self.assertRaises(apply_patch.Error, self._header.Parse, diff_lines)
    diff_lines = [
        '--- dir/file.txt 1969-12-31 17:00:00.000000000 -0700\n',
        '@@ dir/file_new.txt  2010-07-08 09:49:37.000000000 -0600\n'
      ]
    self.assertRaises(apply_patch.Error, self._header.Parse, diff_lines)

class TestPatch(unittest.TestCase):
  """ Test class _Patch. """

  TEST_DIR = 'apply_patch_test_archive'
  TEST_FILE = 'original_file.txt'
  TEST_DATA = [
      'aaaaaaaaaa\n',
      'bbbbbbbbbb\n',
      'cccccccccc\n',
      'dddddddddd\n',
      'eeeeeeeeee\n',
      'ffffffffff\n',
    ]

  def setUp(self):
    self._patch = apply_patch._Patch()

  def testParseAndApply(self):
    """ Test function _Patch.Parse. """
    diff_lines = [
        '--- ' + TestPatch.TEST_DIR + '/' + TestPatch.TEST_FILE + '\n',
        '+++ ' + TestPatch.TEST_DIR + '/new' + TestPatch.TEST_FILE + '\n',
        '@@ -1,0 +1,1 @@\n',
        '+zzzzzzzzzz\n',
        '@@ -3,2 +3,2 @@\n',
        ' cccccccccc\n',
        '-dddddddddd\n',
        '+----------\n',
      ]
    self._patch.Parse(diff_lines)

    # Create the original data file.
    script_dir = os.path.dirname(__file__)
    test_dir = os.path.join(script_dir, TestPatch.TEST_DIR)
    file_path = os.path.join(test_dir, TestPatch.TEST_FILE)
    if not os.path.exists(test_dir):
      os.mkdir(test_dir)
    with open(file_path, 'w+b') as file:
      file.truncate(0)
      file.writelines(TestPatch.TEST_DATA)

    # Apply the patch and verify patched file
    patched_data = [
        'zzzzzzzzzz\n',
        'aaaaaaaaaa\n',
        'bbbbbbbbbb\n',
        'cccccccccc\n',
        '----------\n',
        'eeeeeeeeee\n',
        'ffffffffff\n',
      ]
    self._patch.Apply(script_dir)
    with open(file_path) as file:
      self.assertEqual(file.readline(), patched_data.pop(0))


def RunTests():
  outcome = True
  for test_class in [TestRange, TestChangeHunk, TestPatchHeader, TestPatch]:
    suite = unittest.TestLoader().loadTestsFromTestCase(test_class)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    outcome = outcome and result.wasSuccessful()

  return int(not outcome)


if __name__ == '__main__':
  sys.exit(RunTests())
