#!/usr/bin/python2.6
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for build_utils.py."""

__author__ = 'mball@google.com (Matt Ball)'

import platform
import os
import subprocess
import sys
import tarfile
import unittest

from build_tools import build_utils
import mox


class TestBuildUtils(unittest.TestCase):
  """This class tests basic functionality of the build_utils package"""
  def setUp(self):
    self.mock_factory = mox.Mox()

  def testArchitecture(self):
    """Testing the Architecture function"""
    bit_widths = build_utils.SupportedNexeBitWidths()
    # Make sure word-width of either 32 or 64.
    self.assertTrue(32 in bit_widths or 64 in bit_widths)
    if sys.platform in ['linux', 'linux2']:
      self.assertTrue(32 in bit_widths)
      if '64' in platform.machine():
        self.assertTrue(64 in bit_widths)
    elif sys.platform == 'darwin':
      # Mac should have both 32- and 64-bit support.
      self.assertTrue(32 in bit_widths)
      self.assertTrue(64 in bit_widths)
    else:
      # Windows supports either 32- or 64-bit, but not both.
      self.assertEqual(1, len(bit_widths))

  def testBotAnnotatorPrint(self):
    """Testing the Print function of the BotAnnotator class"""
    stdout_mock = self.mock_factory.CreateMock(sys.stdout)
    stdout_mock.write("My Bot Message\n")
    stdout_mock.flush()
    stdout_mock.write("BUILD_STEP MyBuildStep\n")
    stdout_mock.flush()
    self.mock_factory.ReplayAll()
    bot = build_utils.BotAnnotator(stdout_mock)
    bot.Print("My Bot Message")
    bot.BuildStep("MyBuildStep")
    self.mock_factory.VerifyAll()

  def testBotAnnotatorRun(self):
    """Testing the 'Run' command of the BotAnnotator class"""
    out_string = 'hello'
    print_command = ['python', '-c',
                     "import sys; sys.stdout.write('%s')" % out_string]
    error_command = ['python', '-c', "import sys; sys.exit(1)"]
    stdout_mock = self.mock_factory.CreateMock(sys.stdout)
    stdout_mock.write('Running %s\n' % print_command)
    stdout_mock.flush()
    stdout_mock.write('%s\n' % out_string)
    stdout_mock.flush()
    stdout_mock.write('Running %s\n' % error_command)
    stdout_mock.flush()
    stdout_mock.write('\n')
    stdout_mock.flush()
    self.mock_factory.ReplayAll()
    bot = build_utils.BotAnnotator(stdout_mock)
    run_output = bot.Run(print_command)
    self.assertEqual(run_output, "%s" % out_string)
    self.assertRaises(subprocess.CalledProcessError, bot.Run, error_command)
    self.mock_factory.VerifyAll()

  def testJoinPathToNaClRepo(self):
    """Testing the 'JoinPathToNaClRepo' utility function."""
    # Test an empty arg list.
    test_dir = os.path.join('third_party', 'native_client')
    self.assertEqual(test_dir, build_utils.JoinPathToNaClRepo())
    # Test an empty arg list with just the root_dir key set.
    test_dir = os.path.join('test_root', test_dir)
    self.assertEqual(test_dir,
                     build_utils.JoinPathToNaClRepo(root_dir='test_root'))
    # Test non-empty arg lists and with and without root_dir.
    test_dir = os.path.join('third_party', 'native_client', 'testing', 'file')
    self.assertEqual(test_dir,
                     build_utils.JoinPathToNaClRepo('testing', 'file'))
    test_dir = os.path.join('test_root', test_dir)
    self.assertEqual(test_dir,
        build_utils.JoinPathToNaClRepo('testing', 'file', root_dir='test_root'))


def RunTests():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestBuildUtils)
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(RunTests())
