#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for init_project.py."""

__author__ = 'mlinck@google.com (Michael Linck)'

import fileinput
import os
import shutil
import sys
import unittest
import mox
import init_project


def TestMock(file_path, open_func):
  temp_file = open_func(file_path)
  temp_file.close()


class TestGlobalFunctions(unittest.TestCase):
  """Class for test cases to cover globally declared helper functions."""

  def testGetCamelCaseName(self):
    output = init_project.GetCamelCaseName('camel_case_name')
    self.assertEqual(output, 'CamelCaseName')
    output = init_project.GetCamelCaseName('print_42')
    self.assertEqual(output, 'Print42')

  def testGetCodeDirectory(self):
    output = init_project.GetCodeDirectory(True, '')
    self.assertEqual(output, 'c')
    output = init_project.GetCodeDirectory(False, '')
    self.assertEqual(output, 'cc')
    output = init_project.GetCodeDirectory(True, 'test')
    self.assertEqual(output, 'test/c')
    output = init_project.GetCodeDirectory(False, 'test')
    self.assertEqual(output, 'test/cc')

  def testGetCodeSourceFiles(self):
    output = init_project.GetCodeSourceFiles(False)
    self.assertEqual(output, init_project.CC_SOURCE_FILES)
    output = init_project.GetCodeSourceFiles(True)
    self.assertEqual(output, init_project.C_SOURCE_FILES)

  def testGetCommonSourceFiles(self):
    output = init_project.GetCommonSourceFiles()
    expected_output_linux = init_project.COMMON_PROJECT_FILES
    expected_output_windows = init_project.COMMON_PROJECT_FILES
    expected_output_windows.extend(['scons.bat'])
    linux_match = (output == expected_output_linux)
    windows_match = (output == expected_output_windows)
    passed = (linux_match | windows_match)
    self.assertTrue(passed)

  def testGetHTMLDirectory(self):
    output = init_project.GetHTMLDirectory('')
    self.assertEqual(output, 'html')
    output = init_project.GetHTMLDirectory('test')
    self.assertEqual(output, 'test/html')

  def testGetHTMLSourceFiles(self):
    output = init_project.GetHTMLSourceFiles()
    self.assertEqual(output, init_project.HTML_FILES)

  def testGetTargetFileName(self):
    output = init_project.GetTargetFileName('project_file.cc', 'bonkers')
    self.assertEqual(output, 'bonkers.cc')
    output = init_project.GetTargetFileName('constant.html', 'bonkers')
    self.assertEqual(output, 'constant.html')

  def testParseArguments(self):
    output = init_project.ParseArguments(['-n', 'test_name', '-d', 'test/dir'])
    self.assertEqual(output.is_c_project, False)
    self.assertEqual(output.project_name, 'test_name')
    self.assertEqual(output.project_directory, 'test/dir')
    output = init_project.ParseArguments(['-n', 'test_name_2', '-c'])
    self.assertEqual(output.is_c_project, True)
    self.assertEqual(output.project_name, 'test_name_2')
    self.assertEqual(output.project_directory,
                     init_project.GetDefaultProjectDir())


class TestProjectInitializer(unittest.TestCase):
  """Class for test cases to cover public interface of ProjectInitializer."""

  def setUp(self):
    self.script_dir = os.path.abspath(os.path.dirname(__file__))
    self.nacl_src_dir = os.getenv('NACL_SDK_ROOT', None)
    self.mock_factory = mox.Mox()
    # This mock is only valid for initialization and will be overwritten
    # after ward by self.os_mock.
    init_path_mock = self.mock_factory.CreateMock(os.path)
    init_path_mock.join('test/dir', 'test_project').AndReturn(
        'test/dir/test_project')
    init_path_mock.exists('test/dir/test_project').AndReturn(False)
    init_os_mock = self.mock_factory.CreateMock(os)
    init_os_mock.path = init_path_mock
    init_os_mock.makedirs('test/dir/test_project')
    self.mock_factory.ReplayAll()
    self.test_subject = init_project.ProjectInitializer(
        # True => is C project, False => is vs project
        True, False, 'test_project', 'test/dir', 'pepper_14', self.script_dir,
        init_os_mock)
    self.mock_factory.VerifyAll()
    self.InitializeResourceMocks()

  def InitializeResourceMocks(self):
    """Can be called multiple times if multiple functions need to be tested."""
    self.fileinput_mock = self.mock_factory.CreateMock(fileinput)
    self.test_subject.fileinput = self.fileinput_mock
    self.os_mock = self.mock_factory.CreateMock(os)
    self.test_subject.os = self.os_mock
    self.shutil_mock = self.mock_factory.CreateMock(shutil)
    self.test_subject.shutil = self.shutil_mock
    self.sys_mock = self.mock_factory.CreateMock(sys)
    self.test_subject.sys = self.sys_mock

  def testCopyAndRenameFiles(self):
    self.shutil_mock.copy('source/dir/normal_name.txt',
                          'test/dir/test_project/normal_name.txt')
    self.shutil_mock.copy('source/dir/project_file.txt',
                          'test/dir/test_project/test_project.txt')
    self.os_mock.path = os.path
    self.mock_factory.ReplayAll()
    self.test_subject.CopyAndRenameFiles(
        'source/dir', ['normal_name.txt', 'project_file.txt'])
    self.mock_factory.VerifyAll()

  def testPrepareDirectoryContent(self):
    self.shutil_mock.copy(
        '%s/c/build.scons' % self.script_dir,
        'test/dir/test_project/build.scons')
    self.shutil_mock.copy(
        '%s/c/project_file.c' % self.script_dir,
        'test/dir/test_project/test_project.c')
    self.shutil_mock.copy(
        '%s/html/project_file.html' % self.script_dir,
        'test/dir/test_project/test_project.html')
    self.shutil_mock.copy(
        '%s/scons' % self.script_dir,
        'test/dir/test_project/scons')
    self.shutil_mock.copy(
        '%s/scons.bat' % self.script_dir,
        'test/dir/test_project/scons.bat')
    self.os_mock.path = os.path
    self.mock_factory.ReplayAll()
    self.test_subject.PrepareDirectoryContent()
    self.mock_factory.VerifyAll()

  def testPrepareFileContent(self):
    self.testCopyAndRenameFiles()
    # We need a new set of resource mocks since the old ones have already been
    # used.
    self.InitializeResourceMocks()
    path_mock = self.mock_factory.CreateMock(os.path)
    stdout_mock = self.mock_factory.CreateMock(sys.stdout)
    self.os_mock.path = path_mock
    path_mock.abspath(self.nacl_src_dir).AndReturn(self.nacl_src_dir)
    self.fileinput_mock.input(
        'test/dir/test_project/normal_name.txt',
        inplace=1, mode='U').AndReturn(
            ['A line with <PROJECT_NAME>.',
             'A line with <ProjectName>.',
             'A line with <NACL_SDK_ROOT>.',
             'A line with <NACL_PLATFORM>.'])
    stdout_mock.write('A line with test_project.')
    stdout_mock.write('A line with <ProjectName>.')
    stdout_mock.write('A line with <NACL_SDK_ROOT>.')
    stdout_mock.write('A line with <NACL_PLATFORM>.')
    self.fileinput_mock.input(
        'test/dir/test_project/normal_name.txt',
        inplace=1, mode='U').AndReturn(
            ['A line with test_project.',
             'A line with <ProjectName>.',
             'A line with <NACL_SDK_ROOT>.',
             'A line with <NACL_PLATFORM>.'])
    stdout_mock.write('A line with test_project.')
    stdout_mock.write('A line with TestProject.')
    stdout_mock.write('A line with <NACL_SDK_ROOT>.')
    stdout_mock.write('A line with <NACL_PLATFORM>.')
    self.fileinput_mock.input(
        'test/dir/test_project/normal_name.txt',
        inplace=1, mode='U').AndReturn(
            ['A line with test_project.',
             'A line with TestProject.',
             'A line with <NACL_SDK_ROOT>.',
             'A line with <NACL_PLATFORM>.'])
    stdout_mock.write('A line with test_project.')
    stdout_mock.write('A line with TestProject.')
    stdout_mock.write('A line with %s.' % self.nacl_src_dir)
    stdout_mock.write('A line with <NACL_PLATFORM>.')
    self.fileinput_mock.input(
        'test/dir/test_project/normal_name.txt',
        inplace=1, mode='U').AndReturn(
            ['A line with test_project.',
             'A line with TestProject.',
             'A line with some/dir.',
             'A line with <NACL_PLATFORM>.'])
    stdout_mock.write('A line with test_project.')
    stdout_mock.write('A line with TestProject.')
    stdout_mock.write('A line with some/dir.')
    stdout_mock.write('A line with pepper_14.')
    # One multi-line file with different replacements has already been mocked
    # so we make this next test simpler.
    self.fileinput_mock.input(
        'test/dir/test_project/test_project.txt',
        inplace=1, mode='U').AndReturn(['A line with no replaceable text.'])
    stdout_mock.write('A line with no replaceable text.')
    self.fileinput_mock.input(
        'test/dir/test_project/test_project.txt',
        inplace=1, mode='U').AndReturn(['A line with no replaceable text.'])
    stdout_mock.write('A line with no replaceable text.')
    self.fileinput_mock.input(
        'test/dir/test_project/test_project.txt',
        inplace=1, mode='U').AndReturn(['A line with no replaceable text.'])
    stdout_mock.write('A line with no replaceable text.')
    self.fileinput_mock.input(
        'test/dir/test_project/test_project.txt',
        inplace=1, mode='U').AndReturn(['A line with no replaceable text.'])
    stdout_mock.write('A line with no replaceable text.')
    self.sys_mock.stdout = stdout_mock
    self.mock_factory.ReplayAll()
    self.test_subject.PrepareFileContent()
    self.mock_factory.VerifyAll()

  def testReplaceInFile(self):
    self.fileinput_mock.input('test/path', inplace=1, mode='U').AndReturn(
        ['A sentence replace_me.'])
    stdout_mock = self.mock_factory.CreateMock(sys.stdout)
    stdout_mock.write('A sentence with_this.')
    self.sys_mock.stdout = stdout_mock
    self.mock_factory.ReplayAll()
    self.test_subject.ReplaceInFile('test/path', 'replace_me', 'with_this')
    self.mock_factory.VerifyAll()


def RunTests():
  # It's possible to do this with one suite instead of two, but then it's
  # harder to read the test output.
  return_value = 1
  suite_one = unittest.TestLoader().loadTestsFromTestCase(TestGlobalFunctions)
  result_one = unittest.TextTestRunner(verbosity=2).run(suite_one)
  suite_two = unittest.TestLoader().loadTestsFromTestCase(
      TestProjectInitializer)
  result_two = unittest.TextTestRunner(verbosity=2).run(suite_two)
  if result_one.wasSuccessful() and result_two.wasSuccessful():
    return_value = 0
  return return_value


if __name__ == '__main__':
  sys.exit(RunTests())
