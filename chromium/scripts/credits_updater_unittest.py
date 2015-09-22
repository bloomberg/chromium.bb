#!/usr/bin/python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import credits_updater as cu
import os
import string
import unittest

# Assumes this script is in ffmpeg/chromium/scripts/
SOURCE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          os.path.pardir,
                          os.path.pardir)
OUTPUT_FILE = 'CREDITS.testing'

# Expected credits for swresample.h
SWRESAMPLE_H_LICENSE = '''libswresample/swresample.h

Copyright (C) 2011-2013 Michael Niedermayer (michaelni@gmx.at)

This file is part of libswresample

libswresample is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

libswresample is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with libswresample; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA'''


def NewCreditsUpdater():
  return cu.CreditsUpdater(SOURCE_DIR, OUTPUT_FILE)


class CreditsUpdaterUnittest(unittest.TestCase):

  def tearDown(self):
    # Cleanup the testing output file
    test_credits = os.path.join(SOURCE_DIR, OUTPUT_FILE)
    if os.path.exists(test_credits):
      os.remove(test_credits)

  def testNoFiles(self):
    # Write credits without processing any files.
    NewCreditsUpdater().WriteCredits()

    # Credits should *always* have LICENSE.md followed by full LGPL text.
    expected_lines = NormalizeNewLines(GetLicenseMdLines() +
                                       GetSeparatorLines() +
                                       GetLicenseLines(cu.License.LGPL))
    credits_lines = ReadCreditsLines()
    self.assertEqual(expected_lines, credits_lines)

  def testLPGLFiles(self):
    # Process two known LGPL files
    updater = NewCreditsUpdater()
    updater.ProcessFile('libavformat/mp3dec.c')
    updater.ProcessFile('libavformat/mp3enc.c')
    updater.WriteCredits()

    # Expect output to have just LGPL text (once) preceded by LICENSE.md
    expected_lines = NormalizeNewLines(GetLicenseMdLines() +
                                       GetSeparatorLines() +
                                       GetLicenseLines(cu.License.LGPL))
    credits_lines = ReadCreditsLines()
    self.assertEqual(expected_lines, credits_lines)

  def testKnownBucketFiles(self):
    # Process some JPEG and MIPS files.
    updater = NewCreditsUpdater()
    updater.ProcessFile('libavcodec/jfdctfst.c')
    updater.ProcessFile('libavutil/mips/float_dsp_mips.c')
    updater.WriteCredits()

    # Expected output to have JPEG and MIPS text in addition to the typical LGPL
    # and LICENSE.md header. JPEG should appear before MIPS because known
    # buckets will be printed in alphabetical order.
    expected_lines = NormalizeNewLines(GetLicenseMdLines() +
                                       GetSeparatorLines() +
                                       ['libavcodec/jfdctfst.c\n\n'] +
                                       GetLicenseLines(cu.License.JPEG) +
                                       GetSeparatorLines() +
                                       ['libavutil/mips/float_dsp_mips.c\n\n'] +
                                       GetLicenseLines(cu.License.MIPS) +
                                       GetSeparatorLines() +
                                       GetLicenseLines(cu.License.LGPL))
    credits_lines = ReadCreditsLines()
    self.assertEqual(expected_lines, credits_lines)

  def testGeneratedAndKnownLicences(self):
    # Process a file that doesn't fall into a known bucket (e.g. the license
    # header for this file is unique). Also process a known bucket file.
    updater = NewCreditsUpdater()
    updater.ProcessFile('libswresample/swresample.h')
    updater.ProcessFile('libavutil/mips/float_dsp_mips.c')
    updater.WriteCredits()

    # Expect output to put swresample.h header first, followed by MIPS.
    expected_lines = NormalizeNewLines(GetLicenseMdLines() +
                                       GetSeparatorLines() +
                                       SWRESAMPLE_H_LICENSE.splitlines(True) +
                                       GetSeparatorLines() +
                                       ['libavutil/mips/float_dsp_mips.c\n\n'] +
                                       GetLicenseLines(cu.License.MIPS) +
                                       GetSeparatorLines() +
                                       GetLicenseLines(cu.License.LGPL))
    credits_lines = ReadCreditsLines()
    self.assertEqual(expected_lines, credits_lines)

  def testKnownFileDigestChange(self):
    updater = NewCreditsUpdater()

    # Choose a known file.
    known_file = os.path.join('libavformat', 'oggparseogm.c')
    self.assertTrue(known_file in updater.known_file_map)

    # Show file processing works without raising SystemExit.
    updater.ProcessFile(known_file)

    # Alter the license digest for this file to simulate a change to the
    # license header.
    orig_file_info = updater.known_file_map[known_file]
    altered_file_info = cu.FileInfo(cu.License.LGPL,
                                    'chris' + orig_file_info.license_digest[5:])
    updater.known_file_map[known_file] = altered_file_info

    # Verify digest mismatch triggers SystemExit.
    with self.assertRaises(SystemExit):
      updater.ProcessFile(known_file)


# Globals to cache the text of static files once read.
g_license_md_lines = []
g_license_lines = {}


def ReadCreditsLines():
  with open(os.path.join(SOURCE_DIR, OUTPUT_FILE)) as test_credits:
    return test_credits.readlines()


def GetLicenseMdLines():
  global g_license_md_lines
  if not len(g_license_md_lines):
    with open(os.path.join(SOURCE_DIR, cu.UPSTREAM_LICENSEMD)) as license_md:
      g_license_md_lines = license_md.readlines()
  return g_license_md_lines


def GetLicenseLines(license_file):
  if not license_file in g_license_lines:
    g_license_lines[license_file] = GetFileLines(
        os.path.join(cu.LICENSE_TEXTS[license_file]))
  return g_license_lines[license_file]


def GetFileLines(file_path):
  with open(file_path) as open_file:
    return open_file.readlines()


def GetSeparatorLines():
  # Pass True to preserve \n chars in the return.
  return cu.LICENSE_SEPARATOR.splitlines(True)

# Combine into a string then split back out to a list. This is important for
# making constructed expectations match the credits read from a file. E.g.
# input: ['foo', '\n', 'bar']
# return: ['foo\n', 'bar']
# Comparing lists line by line makes for much better diffs when things go wrong.


def NormalizeNewLines(lines):
  return ''.join(lines).splitlines(True)


if __name__ == '__main__':
  unittest.main()
