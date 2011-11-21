#!/usr/bin/python2.6
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for nsis_script.py."""

import filecmp
import os
import sys
import tarfile
import tempfile
import unittest

from build_tools import nsis_script


class TestNsisScript(unittest.TestCase):
  """This class tests basic functionality of the nsis_script package"""

  def FilterSvn(self, list):
    '''A filter used to remove .svn dirs from installer lists.'''
    return [elt for elt in list if '.svn' not in elt]

  def testConstructor(self):
    """Test default constructor."""
    script = nsis_script.NsisScript('test_script.nsi')
    self.assertEqual('test_script.nsi', script.script_file)
    self.assertEqual(0, len(script.files))
    self.assertEqual(0, len(script.dirs))
    self.assertEqual(0, len(script.symlinks))
    self.assertEqual(0, len(script.links))

  def testInstallDir(self):
    """Test install directory accessor/mutator."""
    script = nsis_script.NsisScript('test_script.nsi')
    test_inst_dir = os.path.join('C:%s' % os.sep, 'test_install_dir')
    script.install_dir = test_inst_dir
    self.assertEqual(test_inst_dir, script.install_dir)

  def testBadInstallDir(self):
    """Test install directory mutator with bad data"""
    script = nsis_script.NsisScript('test_script.nsi')
    try:
      script.install_dir = 'bogus_path'
      self.fail('install_dir failed to throw an exception with bogus_path')
    except nsis_script.Error:
      pass
    else:
      raise

  def testInitFromDirectory(self):
    """Test creation of artifact lists from an archive directory."""
    script = nsis_script.NsisScript('test_script.nsi')
    archive_dir = os.path.join('build_tools', 'tests', 'nsis_test_archive')
    script.InitFromDirectory(archive_dir,
                             dir_filter=self.FilterSvn,
                             file_filter=self.FilterSvn)
    file_set = script.files
    self.assertEqual(3, len(file_set))
    self.assertTrue(os.path.join(archive_dir, 'test_file.txt') in file_set)
    self.assertTrue(os.path.join(archive_dir, 'test_dir', 'test_dir_file1.txt')
        in file_set)
    self.assertTrue(os.path.join(archive_dir, 'test_dir', 'test_dir_file2.txt')
        in file_set)
    dir_set = script.dirs
    self.assertEqual(1, len(dir_set))
    self.assertTrue(os.path.join(archive_dir, 'test_dir') in dir_set)

  def testCreateInstallNameScript(self):
    """Test the install name include script."""
    test_dir = os.path.join('build_tools', 'tests')
    script = nsis_script.NsisScript(os.path.join(test_dir, 'test_script.nsi'))
    script.CreateInstallNameScript(cwd=test_dir)
    install_name_script = open(os.path.join(test_dir, 'sdk_install_name.nsh'),
                               'r')
    self.assertTrue(script.install_dir in install_name_script.read())
    install_name_script.close()
    os.remove(os.path.join(test_dir, 'sdk_install_name.nsh'))

  def testNormalizeInstallPath(self):
    """Test NormalizeInstallPath."""
    # If InitFromDirectory() is not called, then install paths are unchanged.
    test_dir = os.path.join('build_tools', 'tests')
    script = nsis_script.NsisScript(os.path.join(test_dir, 'test_script.nsi'))
    test_path = os.path.join('C:', 'test', 'path')
    path = script.NormalizeInstallPath(test_path)
    self.assertEqual(test_path, path)
    # Set a relative install path.
    archive_dir = os.path.join(test_dir, 'nsis_test_archive')
    script.InitFromDirectory(archive_dir,
                               dir_filter=self.FilterSvn,
                               file_filter=self.FilterSvn)
    test_path = os.path.join('test', 'relative', 'path')
    path = script.NormalizeInstallPath(os.path.join(archive_dir, test_path))
    self.assertEqual(test_path, path)

  def testCreateSectionNameScript(self):
    """Test the section name script."""
    test_dir = os.path.join('build_tools', 'tests')
    script = nsis_script.NsisScript(os.path.join(test_dir, 'test_script.nsi'))
    archive_dir = os.path.join(test_dir, 'nsis_test_archive')
    script.InitFromDirectory(archive_dir,
                               dir_filter=self.FilterSvn,
                               file_filter=self.FilterSvn)
    script.CreateSectionNameScript(cwd=test_dir)
    # When comparing the contents of the script with the golden file, note
    # that the 'File' lines can be in any order.  All the other section commands
    # have to be in the correct order.
    def GetSectionCommands(section_script):
      '''Split all the section commands into File and other commands.

      Returns:
        A tuple (commands, file_set), where |commands| is an ordered list of
        NSIS section commands, and |file_set| is an unordered set of NSIS File
        commands.
      '''
      commands = []
      file_set = set()
      with open(section_script) as script_file:
        commands = script_file.readlines();  # All the commands in order.
        file_set = set([file_cmd
                        for file_cmd in commands if 'File ' in file_cmd])
        # Remove the File commands from the in-order command list.
        for file in file_set:
          commands.remove(file)
      return commands, file_set

    test_commands, test_files = GetSectionCommands(
        os.path.join(test_dir, 'test_sdk_section.nsh'))
    script_commands, script_files = GetSectionCommands(
        os.path.join(test_dir, 'sdk_section.nsh'))
    self.assertEqual(test_commands, script_commands)
    self.assertEqual(test_files, script_files)  # Uses set() equality.
    os.remove(os.path.join(test_dir, 'sdk_section.nsh'))


def RunTests():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestNsisScript)
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())


if __name__ == '__main__':
  sys.exit(RunTests())
