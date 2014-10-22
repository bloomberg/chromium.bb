#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for Copyright Scanner utilities."""

import os
import re
import sys
import unittest

test_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.extend([
    os.path.normpath(os.path.join(test_dir, '..', '..', 'tools')),
    os.path.join(test_dir),
])

import find_depot_tools
from testing_support.super_mox import SuperMoxTestBase

import copyright_scanner

class FindCopyrightsTest(SuperMoxTestBase):
  def setUp(self):
    SuperMoxTestBase.setUp(self)
    self.input_api = self.mox.CreateMockAnything()
    self.input_api.re = re
    self.input_api.os_path = os.path
    self.input_api.os_walk = os.walk

  def ShouldMatchReferenceOutput(self, test_data, expected_output):
    for data in test_data:
      self.input_api.ReadFile = lambda _1, _2: data
      actual_output = copyright_scanner.FindCopyrights(self.input_api, '', [''])
      self.assertEqual(
        expected_output,
        actual_output,
        'Input """\n%s""", expected output: "%s", actual: "%s"' % \
            (data, expected_output, actual_output));

  def testCopyrightedFiles(self):
    test_data = [
      '// (c) 2014 Google Inc.\n//\n//  (a) One\n//\n//  (b) Two\n//\n',
      'Copyright 2014 Google Inc.\n',
      'Copr. 2014 Google Inc.',
      '\xc2\xa9 2014 Google Inc.',
      'Copyright 2014    Google  Inc.'
    ]
    self.ShouldMatchReferenceOutput(test_data, [['2014 Google Inc.']])

  def testGeneratedFiles(self):
    test_data = [
      'ALL CHANGES MADE IN THIS FILE WILL BE LOST\nCopyright 2014 Google\n',
      'GENERATED FILE. DO NOT EDIT\nCopyright 2014 Google\n',
      'GENERATED. DO NOT DELETE THIS FILE.\nCopyright 2014 Google\n',
      'DO NOT EDIT\nCopyright 2014 Google\n',
      'DO NOT DELETE THIS FILE\nCopyright 2014 Google\n',
      'All changes made in this file will be lost\nCopyright 2014 Google\n',
      'Automatically generated file\nCopyright 2014 Google\n',
      'Synthetically generated dummy file\nCopyright 2014 Google\n',
      'Generated data (by gnugnu)\nCopyright 2014 Google\n'
    ]
    self.ShouldMatchReferenceOutput(test_data, [['GENERATED FILE']])

  def testNonCopyrightedFiles(self):
    test_data = [
      'std::cout << "Copyright 2014 Google"\n',
      '// Several points can be made:\n//\n//  (a) One\n//\n//  (b) Two\n'
      '//\n//  (c) Three\n//\n',
      'See \'foo\' for copyright information.\n',
      'See \'foo\' for the copyright notice.\n',
      'See \'foo\' for the copyright and other things.\n'
    ]
    self.ShouldMatchReferenceOutput(test_data, [['*No copyright*']])

  def testNonGeneratedFiles(self):
    test_data = [
      'This file was prohibited from being generated.\n',
      'Please do not delete our files! They are valuable to us.\n',
      'Manually generated from dice rolls.\n',
      '"""This Python script produces generated data\n"""\n',
      '\'\'\'This Python script produces generated data\n\'\'\'\n'
    ]
    self.ShouldMatchReferenceOutput(test_data, [['*No copyright*']])

if __name__ == '__main__':
  unittest.main()
