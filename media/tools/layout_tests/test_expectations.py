#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for TestExpectionsManager.

TestExpectaionManager manages data for Webkit layout tests.
"""

import csv
import re
import time
import urllib
import pysvn

from csv_utils import CsvUtils
from layout_test_test_case import LayoutTestCaseManager
from test_case_patterns import TestCasePatterns


class TestExpectationsManager(object):
    """This class manages test expectations data for Webkit layout tests.

    The detail of the test expectation file can be found in
    http://trac.webkit.org/wiki/TestExpectations.

    This class does the following:
    (1) get test expectation file from WebKit subversion source
        repository using pysvn.
    (2) parse the file and generate CSV entries.
    (3) create hyperlinks where appropriate (e.g., test case name).
    """

    DEFAULT_TEST_EXPECTATION_DIR = (
        'http://svn.webkit.org/repository/webkit/trunk/'
        'LayoutTests/platform/chromium/')

    DEFAULT_TEST_EXPECTATION_LOCATION = (
        'http://svn.webkit.org/repository/webkit/trunk/'
        'LayoutTests/platform/chromium/test_expectations.txt')

    CHROME_BUG_URL = 'http://code.google.com/p/chromium/issues/detail?id='

    WEBKIT_BUG_URL = 'https://bugs.webkit.org/show_bug.cgi?id='

    FLAKINESS_DASHBOARD_LINK = (
        'http://test-results.appspot.com/dashboards/'
        'flakiness_dashboard.html#tests=')

    TEST_CASE_PATTERNS = TestCasePatterns()

    OTHER_FIELD_NAMES = ['TestCase', 'Media', 'Flaky', 'WebKitD', 'Bug',
                         'Test']

    # The following is from test expectation syntax.
    # BUG[0-9]+ [SKIP] [WONTFIX] [SLOW]
    DECISION_COLUMN_NAMES = ['SKIP', 'SLOW']
    # <platform> ::== [GPU] [CPU] [WIN] [LINUX] [MAC]
    PLATFORM_COLUMN_NAMES = ['GPU', 'CPU', 'WIN', 'LINUX', 'MAC']
    #<config> ::== RELEASE | DEBUG
    CONFIG_COLUMN_NAMES = ['RELEASE', 'DEBUG']
    # <EXPECTATION_COLUMN_NAMES> ::== \
    #   [FAIL] [PASS] [CRASH] [TIMEOUT] [IMAGE] [TEXT] [IMAGE+TEXT]
    EXPECTATION_COLUMN_NAMES = ['FAIL', 'PASS', 'CRASH',
                                'TIMEOUT', 'IMAGE', 'TEXT',
                                'IMAGE+TEXT']

    # These are coming from metadata in comments in the test expectation file.
    COMMENT_COLUMN_NAMES = ['UNIMPLEMENTED', 'KNOWNISSUE', 'TESTISSUE',
                            'WONTFIX']

    RAW_COMMENT_COLUMN_NAME = 'COMMENTS'

    def __init__(self):
        """Initialize the test cases."""
        self.testcases = []

    def get_column_indexes(self, column_names):
        """Get column indexes for given column names.

        Args:
            column_names: a list of column names.

        Returns:
            a list of indexes for the given column names.
        """
        all_field_names = self.get_all_column_names(True, True)
        return [all_field_names.index(
            field_name) for field_name in column_names]

    def get_all_column_names(self, include_other_fields, include_comments):
        """Get all column names that are used in CSV file.

        Args:
            include_other_fields: a boolean that indicates the result should
                include other column names (defined in OTHER_COLUMNS_NAMES).
            include_comments: a boolean that indicates the result should
                include comment-related names.

        Returns:
            a list that contains column names.
        """
        return_list = (
            self.DECISION_COLUMN_NAMES + self.PLATFORM_COLUMN_NAMES +
            self.CONFIG_COLUMN_NAMES + self.EXPECTATION_COLUMN_NAMES)
        if include_other_fields:
            return_list = self.OTHER_FIELD_NAMES + return_list
        if include_comments:
            return_list.extend(self.COMMENT_COLUMN_NAMES)
            return_list.append(self.RAW_COMMENT_COLUMN_NAME)
        return return_list

    def get_test_case_element(self, test_case, column_names):
        """Get test case elements.

        A test case is a collection of test case elements.

        Args:
            test_case: test case data that contains all test case elements.
            column_names: column names for specifying test case elements.

        Returns:
            A list of test case elements for the given column names.
        """
        field_indexes = self.get_column_indexes(column_names)
        test_case = self.get_test_case_by_name(test_case)
        if test_case is None:
            return []
        return [test_case[fid] for fid in field_indexes]

    def get_test_case_by_name(self, target):
        """Get test case object by test case name.

        Args:
            target: test case name.

        Returns:
            A test case with the test case name or None if the test case
                cannot be found.
        """
        for testcase in self.testcases:
            # Test case name is stored in the first column.
            if testcase[0] == target:
                return testcase
        return None

    def get_all_test_case_names(self):
        """Get all test case names.

        Returns:
            A list of test case names.
        """
        return [testcase[0] for testcase in self.testcases]

    def generate_link_for_bug(self, bug):
        """Generate link for a bug.

        Parse the bug description accordingly. The bug description can be like
        the following: BUGWK1234, BUGCR1234, BUGFOO.

        Args:
            bug: A string bug description

        Returns:
            A string that represents a bug link. Returns an empty
                string if match is not found.
        """
        pattern_for_webkit_bug = 'BUGWK(\d+)'
        match = re.search(pattern_for_webkit_bug, bug)
        if match is not None:
            return self.WEBKIT_BUG_URL + match.group(1)
        pattern_for_chrome_bug = 'BUGCR(\d+)'
        match = re.search(pattern_for_chrome_bug, bug)
        if match is not None:
            return self.CHROME_BUG_URL + match.group(1)
        pattern_for_other_bug = 'BUG(\S+)'
        match = re.search(pattern_for_other_bug, bug)
        if match is not None:
            return 'mailto:%s@chromium.org' % match.group(1).lower()
        return ''

    def generate_link_for_dashboard(self, testcase_name, webkit):
        """Generate link for flakiness dashboard.

        Args:
            testcase_name: a string test case name.
            webkit: a boolean indicating whether to use the webkit dashboard.

        Returns:
            A string link to the flakiness dashboard.
        """
        url = self.FLAKINESS_DASHBOARD_LINK + urllib.quote(testcase_name)
        if webkit:
            url += '&master=webkit.org'
        return url

    def parse_line(self, line, previous_comment_info_list, test_case_patterns,
                   media_test_cases_only, writer):
        """Parse each line in test_expectations.txt.

        The format of each line is as follows:
            BUG[0-9]+ [SKIP] [WONTFIX] [SLOW] [<platform>] [<config>]
              : <url> = <EXPCTATION>
        The example is:
                BUG123 BUG345 MAC : media/hoge.html
                WONTFIX SKIP BUG19635 : media/restore-from-page-cache.html
                    = TIMEOUT

        Args:
          line: a text for each line in the test expectation file.
          previous_comment_info_list: a list of comments in previous lines.
          test_case_patterns: a string for test case pattern.
          media_test_cases_only: a boolean for media test case only.
          writer: writer for intermediate results.
        """
        bugs = re.findall(r'BUG\w+', line)
        testcases = re.search(r':\s+(\S+.[html|xml|svg|js])', line)
        if testcases is not None:
            testcase = testcases.group(1)
            data = []
            data.append(testcase)

            if LayoutTestCaseManager.check_test_case_matches_pattern(
                testcase, test_case_patterns):
                data.append('Y')
                media_test_case = True
            else:
                data.append('N')
                media_test_case = False

            dlink = self.generate_link_for_dashboard(testcase, False)
            # Generate dashboard link.
            data.append(
                CsvUtils.generate_hyperlink_in_csv(dlink, 'Y', ''))
            dlink = self.generate_link_for_dashboard(testcase, True)
            # Generate dashboard link.
            data.append(
                CsvUtils.generate_hyperlink_in_csv(dlink, 'Y', ''))
            for bug in bugs:
                blink = self.generate_link_for_bug(bug)
                data.append(
                    CsvUtils.generate_hyperlink_in_csv(blink, bug, ''))
            # Fill the gap for test case with less bug/test.
            for i in range(2 - len(bugs)):
                data.append('')

            # Fill all data with 'N' (default).
            for field_name in self.get_all_column_names(False, False):
                if field_name in line:
                    data.append('Y')
                else:
                    data.append('N')
            # Comments will be accumulated in previous comments.
            # This is for multiple-line comment.
            for previous_comment_info in previous_comment_info_list:
                data.append(previous_comment_info)
            # Write only media related test case if specified.
            if (media_test_case and media_test_cases_only
                or (not media_test_cases_only)):
                writer.writerow(data)
                self.testcases.append(data)

    def process_comments(self, comments):
        """Process comments in the test expectations file.

        Comments may contain special keywords such as UNIMPLEMENTED,
        KNOWNISSUE, TESTISSUE (in TestExpectationsManager.COMMENT_COLUMN_NAMES)
        and may be multiple lines.

        Args:
            comments: A raw comment from the test expectation file.
               It is above test case name.

        Returns:
            A list of 'Y' or 'N' whether the comments contain
            each column name in TestExpectationsManager.COMMENT_COLUMN_NAMES
            in the comments.
        """
        return_list = []
        for ccn in TestExpectationsManager.COMMENT_COLUMN_NAMES:
            if ccn in comments:
                return_list.append('Y')
            else:
                return_list.append('N')
        return_list.append(comments)
        return return_list

    def get_and_save_content(self, location, output):
        """Simply get test expectation from the specified location and save it.

        Args:
            location: SVN location of the test expectation file.
            output: an output file path including file name.
        """
        client = pysvn.Client()
        file_object = open(output, 'w')
        file_object.write(client.cat(location))
        file_object.close()

    def get_and_save_content_media_only(self, location, output):
        """Simply get test expectation from the specified location.

        It also saves it (media only).

        Args:
            location: SVN location of the test expectation file.
            output: an output file path including file name.
        """
        file_object = file(location, 'r')
        text_list = list(file_object)
        output_text = ''
        for txt in text_list:
            if txt.startswith('//'):
                output_text += txt
            else:
                for pattern in (
                    self.TEST_CASE_PATTERNS.get_test_case_pattern(
                        'media')):
                    if re.search(pattern, txt) is not None:
                        output_text += txt
                        break
        file_object.close()
        # Save output.
        file_object = open(output, 'w')
        file_object.write(output_text)
        file_object.close()

    def get_and_parse_content(self, location, output, media_test_case_only):
        """Get test_expectations.txt and parse the content.

        The comments are parsed as well since it contains keywords.

        Args:
            location: SVN location of the test expectation file.
            output: an output file path including file name.
            media_test_case_only: A boolean indicating whether media
                test cases should only be processed.
        """
        if location.startswith('http'):
            # Get from SVN.
            client = pysvn.Client()
            # Check out the current version of the pysvn project.
            txt = client.cat(location)
        else:
            if location.endswith('.csv'):
                # No parsing in the case of CSV file.
                # Direct reading.
                file_object = file(location, 'r')
                self.testcases = list(csv.reader(file_object))
                file_object.close()
                return
            file_object = open(location, 'r')
            txt = file_object.read()
            file_object.close()

        file_object = file(output, 'wb')
        writer = csv.writer(file_object)
        writer.writerow(self.get_all_column_names(True, True))
        lines = txt.split('\n')
        process = False
        previous_comment = ''
        previous_comment_list = []
        for line in lines:
            if line.isspace() or str is '':
                continue
            line.strip()
            if line.startswith('//'):
                # There are comments.
                line = line.replace('//', '')
                if process is True:
                    previous_comment = line
                else:
                    previous_comment = previous_comment + '\n' + line
                previous_comment_list = self.process_comments(previous_comment)
                process = False
            else:
                self.parse_line(
                    line, previous_comment_list,
                    self.TEST_CASE_PATTERNS.get_test_case_pattern('media'),
                    media_test_case_only, writer)
                process = True
        file_object.close()

    def get_te_diff_between_times(self, te_location, start, end, patterns,
                                  change, checkchange):
        """Get test expectation diff output for a given time period.

        Args:
            te_location: SVN location of the test expectation file.
            start: a date object for the start of the time period.
            end: a date object for the end of the time period.
            patterns: test case name patterns of the test cases that
                we are interested in. This could be test case name
                as it is in the case of exact matching.
            change: the number of change in occurrence of test case in
                test expectation.
            checkchange: a boolean to indicate if we want to check the
                change in occurrence as specified in |change| argument.

        Returns:
            a list of tuples (old_revision, new_revision,
                diff_line, author, date, commit_message) that matches
                the condition.
        """
        client = pysvn.Client()
        client.checkout(te_location, 'tmp', recurse=False)
        logs = client.log('tmp/test_expectations.txt',
                          revision_start=pysvn.Revision(
                              pysvn.opt_revision_kind.date, start),
                          revision_end=pysvn.Revision(
                              pysvn.opt_revision_kind.date, end))
        result_list = []
        for i in xrange(len(logs) - 1):
            # PySVN.log returns logs in reverse chronological order.
            new_rev = logs[i].revision.number
            old_rev = logs[i + 1].revision.number
            # Getting information about new revision.
            author = logs[i].author
            date = logs[i].date
            message = logs[i].message
            text = client.diff('/tmp', 'tmp/test_expectations.txt',
                               revision1=pysvn.Revision(
                                   pysvn.opt_revision_kind.number, old_rev),
                               revision2=pysvn.Revision(
                                   pysvn.opt_revision_kind.number, new_rev))
            lines = text.split('\n')
            for line in lines:
                for pattern in patterns:
                    matches = re.findall(pattern, line)
                    if matches:
                        if checkchange:
                            if ((line[0] == '+' and change > 0) or
                                (line[0] == '-' and change < 0)):
                                result_list.append((old_rev, new_rev, line,
                                                    author, date, message))
                        else:
                            if line[0] == '+' or line[0] == '-':
                                result_list.append((old_rev, new_rev, line,
                                                    author, date, message))
        return result_list
