#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from StringIO import StringIO
import makecab
import unittest
import sys
import time

# Test flag parsing.
class ParseFlagsTest(unittest.TestCase):
  def testInputOnly(self):
    flags = makecab.ParseFlags(['/V1', '/D', 'CompressionType=LZX', 'foo.txt'])
    self.assertEquals(flags.input, 'foo.txt')
    self.assertEquals(flags.output, 'foo.tx_')
    self.assertEquals(flags.output_dir, '.')

  def testInputOutput(self):
    flags = makecab.ParseFlags(['bar.txt', 'd/foo.cab'])
    self.assertEquals(flags.input, 'bar.txt')
    self.assertEquals(flags.output, 'd/foo.cab')
    self.assertEquals(flags.output_dir, '.')

  def testInputOutdir(self):
    flags = makecab.ParseFlags(['/L', 'outdir', 'baz.txt'])
    self.assertEquals(flags.input, 'baz.txt')
    self.assertEquals(flags.output, 'baz.tx_')
    self.assertEquals(flags.output_dir, 'outdir')

  def testInputOutputOutdir(self):
    flags = makecab.ParseFlags(['/L', 'outdir', 'foo.txt', 'd/foo.cab'])
    self.assertEquals(flags.input, 'foo.txt')
    self.assertEquals(flags.output, 'd/foo.cab')
    self.assertEquals(flags.output_dir, 'outdir')


# Test that compression doesn't throw, and on Windows also check that
# expand.exe is able to recover input data.
class WriteCabTest(unittest.TestCase):
  def testWellCompressingInput(self):
    input_data = 'a' * (4 << 15)
    output = StringIO()
    mtime = time.mktime((2018, 1, 8,  16, 00, 00,  0, 8, -1))
    makecab.WriteCab(output, StringIO(input_data), 'a.txt',
                     len(input_data), mtime)
    if sys.platform == 'win32':
      import os, subprocess, tempfile
      cab_handle, cab_path = tempfile.mkstemp(suffix='.makecab_test.cab')
      os.write(cab_handle, output.getvalue())
      os.close(cab_handle)
      out_handle, out_path = tempfile.mkstemp(suffix='.makecab_test.out')
      # Closing out_handle immediately defeats the purpose of mkstemp(), but I
      # can't figure out how to write to the temp file on Windows otherwise.
      os.close(out_handle)
      FNULL = open(os.devnull, 'w')
      subprocess.check_call(['expand.exe', cab_path, out_path], stdout=FNULL)
      self.assertEquals(input_data, open(out_path, 'rb').read())
      os.remove(cab_path)
      os.remove(out_path)


if __name__ == '__main__':
  unittest.main()
