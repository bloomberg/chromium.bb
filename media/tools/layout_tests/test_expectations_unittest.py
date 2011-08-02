#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A unit test module for TestExpectationManager."""

from datetime import date
from datetime import datetime
from datetime import timedelta
import os
import shutil
import time
import unittest

from test_expectations import TestExpectationsManager


class TestTestExpectationsManager(unittest.TestCase):
    """A unit test class for TestExpectationManager."""
    test_dir = u'test_dir'
    cleaned = False

    def setUp(self):
        if not TestTestExpectationsManager.cleaned:
            if os.path.exists(self.test_dir):
                shutil.rmtree(self.test_dir)
            os.mkdir(self.test_dir)
        TestTestExpectationsManager.cleaned = True

    def verify_test_expectaion_format(self, file_name, media):
        """Verify the test expectation format in the file.

        Args:
             file_name: a file name to be verified.
             media: a boolean to indicate whether the test is for media
                 or not.

        Returns:
             a boolean indicating whether the file is in test expectation
                 format.
        """
        file_object = open(file_name, 'r')
        firstline = file_object.readline()
        if ('These are the layout test expectations for the '
            'Chromium port(s) of WebKit.') not in firstline:
            return False
        # Check media related test case file is there.
        verify_media = not media
        if media:
            for line in file_object:
                if 'media' in line:
                    verify_media = True
        file_object.close()
        return verify_media

    def verify_test_expectaion_format_in_csv(self, file_name):
        """Verify the CSV file has information parsed.

        Args:
            file_name: file name to be examined.

        Returns:
            a boolean that indicates that the CSV file contains information
                parsed from test expectation file.
        """
        file_object = open(file_name, 'r')
        firstline = file_object.readline()
        file_object.close()
        return 'TestCase' in firstline

    def test_get_and_save_content(self):
        """Test get_and_save_content method.

        Also, test several other methods.
        """
        # Get all test cases.
        result_file = os.path.join(self.test_dir,
                                   'test_get_and_save_content.txt')

        test_expectations_manager = TestExpectationsManager()
        test_expectations_manager.get_and_save_content(
            test_expectations_manager.DEFAULT_TEST_EXPECTATION_LOCATION,
            result_file)

        self.assertTrue(
            self.verify_test_expectaion_format(result_file, False))

        # Get media test cases from local all test cases.
        result_file_media = os.path.join(self.test_dir,
                                         'test_get_and_save_content_m.txt')

        test_expectations_manager = TestExpectationsManager()
        test_expectations_manager.get_and_save_content_media_only(
            result_file, result_file_media)

        self.assertTrue(
            self.verify_test_expectaion_format(result_file_media, True))

        # Get media test cases from local all test cases.
        result_file_media_csv = os.path.join(
            self.test_dir, 'test_get_and_save_content_m.csv')

        test_expectations_manager.get_and_parse_content(
            result_file, result_file_media_csv, True)

        elements = test_expectations_manager.get_test_case_element(
            'media/restore-from-page-cache.html',
            ['TEXT', 'UNIMPLEMENTED', 'RELEASE'])
        self.assertEqual(
            elements, ['N', 'N', 'N'],
            'returned element of GetTestCaseElement is wrong')

        field_names = ['SKIP', 'UNIMPLEMENTED', 'KNOWNISSUE']
        field_name_indexes = test_expectations_manager.get_column_indexes(
            field_names)
        expected_filed_name_indexes = [6, 22, 23]
        self.assertEqual(field_name_indexes, expected_filed_name_indexes,
                         'incorrect field indexes')

    def test_parse_content_from_svn(self):
        """Test get_and_parse_content method using SVN."""
        self.parse_content_from_svn_test_helper(False)

    def test_parse_content_from_svn_media(self):
        """Test get_and_parse_content method using SVN (only media)."""
        self.parse_content_from_svn_test_helper(True)

    def parse_content_from_svn_test_helper(self, media):
        """Test get_and_parse_content method using SVN.

        Args:
            media: True if this is for media tests.
        """
        media_string = 'all'
        if media:
            media_string = 'media'
        result_file = os.path.join(
            self.test_dir,
            'test_parse_content_from_svn_%s.csv' % media_string)
        test_expectations_manager = TestExpectationsManager()
        test_expectations_manager.get_and_parse_content(
            test_expectations_manager.DEFAULT_TEST_EXPECTATION_LOCATION,
            result_file, media)

        self.assertTrue(test_expectations_manager.testcases > 0)

        self.verify_test_expectaion_format_in_csv(result_file)

    def test_generate_link_for_cbug(self):
        """Test generate_link_for_bug for a chromium bug."""
        self.generate_link_for_bug_helper(
            'BUGCR1234',
            'http://code.google.com/p/chromium/issues/detail?id=1234')

    def test_generate_link_for_wbug(self):
        """Test generate_link_for_bug for a webkit bug."""
        self.generate_link_for_bug_helper(
            'BUGWK1234',
            'https://bugs.webkit.org/show_bug.cgi?id=1234')

    def test_generate_link_for_pbug(self):
        """Test generate_link_for_bug for person."""
        self.generate_link_for_bug_helper('BUGIMASAKI',
                                          'mailto:imasaki@chromium.org')

    def generate_link_for_bug_helper(self, bug_text, expected_link):
        """A helper for generating link test."""
        test_expectations_manager = TestExpectationsManager()
        link = test_expectations_manager.generate_link_for_bug(bug_text)
        self.assertEqual(link, expected_link, 'link generated are not correct')

    def test_te_diff_between_times(self):
        test_expectations_manager = TestExpectationsManager()
        now = time.time()
        past = datetime.now() - timedelta(days=4)
        past = time.mktime(past.timetuple())
        result_list = test_expectations_manager.get_te_diff_between_times(
            test_expectations_manager.DEFAULT_TEST_EXPECTATION_DIR,
            now, past, ['media/audio-repaint.html'], -1, True)
        self.assertTrue(result_list is not None)

    def test_te_diff_between_times_no_changecheck(self):
        test_expectations_manager = TestExpectationsManager()
        now = time.time()
        past = datetime.now() - timedelta(weeks=2)
        past = time.mktime(past.timetuple())
        result_list = test_expectations_manager.get_te_diff_between_times(
            test_expectations_manager.DEFAULT_TEST_EXPECTATION_DIR,
            now, past, ['media/audio-repaint.html'], -1, False)
        self.assertTrue(result_list is not None)

    def test_te_diff_between_times_no_changecheck_with_pattern(self):
        test_expectations_manager = TestExpectationsManager()
        now = time.time()
        past = datetime.now() - timedelta(weeks=2)
        past = time.mktime(past.timetuple())
        result_list = test_expectations_manager.get_te_diff_between_times(
            test_expectations_manager.DEFAULT_TEST_EXPECTATION_DIR,
            now, past, ['media/audio-repaint.html'], -1, False)
        self.assertTrue(result_list is not None)

    def test_get_all_column_names_no_other_field(self):
        test_expectations_manager = TestExpectationsManager()
        column_names = test_expectations_manager.get_all_column_names(False,
                                                                      True)
        other_field_column_names = test_expectations_manager.OTHER_FIELD_NAMES
        for other_field_column_name in other_field_column_names:
            self.assertFalse(other_field_column_name in column_names)

    def test_get_all_column_names_no_comment_field(self):
        test_expectations_manager = TestExpectationsManager()
        column_names = test_expectations_manager.get_all_column_names(True,
                                                                      False)
        other_field_column_names = (
            test_expectations_manager.COMMENT_COLUMN_NAMES)
        for other_field_column_name in other_field_column_names:
            self.assertFalse(other_field_column_name in column_names)


def main():
    test_suite = unittest.TestLoader().loadTestsFromTestCase(
        TestTestExpectationsManager)

    unittest.TextTestRunner(verbosity=2).run(test_suite)
