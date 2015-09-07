# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Encapsulates running tests defined in tests.py.

Running this script requires passing --config-path with a path to a config file
of the following structure:

  [data_files]
  passwords_path=<path to a file with passwords>
  [binaries]
  chrome-path=<chrome binary path>
  chromedriver-path=<chrome driver path>
  [run_options]
  # |tests_in_parallel| is optional, the default value is 1.
  tests_in_parallel=<number of parallel tests>
  # |tests_to_runs| field is optional, if it is absent all tests will be run.
  tests_to_run=<test names to run, comma delimited>
  # |test_cases_to_run| field is optional, if it is absent all test cases
  # will be run.
  test_cases_to_run=<test names to run, comma delimited>
  [logging]
  # |save-only-failures| is optional, the default is false.
  save-only-failures=<Boolean parameter which enforces saving results of only
                      failed tests>
  |permanent-log-file| is a file name of file, where test run results will be
  saved in irder to compare them with a next run. Should be specified if it's
  expected that mail will be send.

The script uses the Python's logging library to report the test results,
as well as debugging information. It emits three levels of logs (in
descending order of severity):
  logging.INFO: Summary of the tests.
  logging.DEBUG: Details about tests failures.
  SCRIPT_DEBUG (see below): Debug info of this script.
You have to set up appropriate logging handlers to have the logs appear.
"""

import ConfigParser
import Queue
import argparse
import logging
import multiprocessing
import os
import shutil
import stopit
import simplejson as json
import tempfile
import time

from threading import Thread

import tests


# Just below logging.DEBUG, use for this script's debug messages instead
# of logging.DEBUG, which is already used for detailed test debug messages.
SCRIPT_DEBUG = 9

class Config:
  test_cases_to_run = tests.TEST_CASES
  save_only_fails = False
  tests_to_run = tests.all_tests.keys()
  max_tests_in_parallel = 1
  permanent_log_file = ""

  def __init__(self, config_path):
    config = ConfigParser.ConfigParser()
    config.read(config_path)
    if config.has_option("run_options", "tests_in_parallel"):
      self.max_tests_in_parallel = config.getint(
          "run_options", "tests_in_parallel")

    self.chrome_path = config.get("binaries", "chrome-path")
    self.chromedriver_path = config.get("binaries", "chromedriver-path")
    self.passwords_path = config.get("data_files", "passwords_path")

    if config.has_option("run_options", "tests_to_run"):
      self.tests_to_run = config.get("run_options", "tests_to_run").split(",")

    if config.has_option("run_options", "test_cases_to_run"):
      self.test_cases_to_run = config.get(
          "run_options", "test_cases_to_run").split(",")
    if config.has_option("logging", "save-only-fails"):
      self.save_only_fails = config.getboolean("logging", "save-only-fails")

    if config.has_option("logging", "permanent-log-file"):
      self.permanent_log_file = config.get("logging", "permanent-log-file")

def LogResultsOfTestRun(config, results):
  """ Logs |results| of a test run. """
  logger = logging.getLogger("run_tests")
  failed_tests = []
  failed_tests_num = 0
  for result in results:
    website, test_case, success, reason = result
    if not (config.save_only_fails and success):
      if not success:
        logger.debug("%s.%s failed with reason: %s",
                     website, test_case, reason)

    if not success:
      failed_tests.append("%s.%s" % (website, test_case))
      failed_tests_num += 1

  failed_tests = sorted([name for name in failed_tests])
  logger.info("%d failed test cases out of %d, failing test cases: %s",
              failed_tests_num, len(results), failed_tests)
  if not config.permanent_log_file:
    return
  diff_failed = []
  with open(config.permanent_log_file, "r+") as log:
    try:
      runs_data = json.load(log)
    except ValueError:
      runs_data = {}
    prev_run_failed = runs_data.get("previous_run", [])
    diff_failed = list(set(failed_tests) - set(prev_run_failed))
    log.seek(0)
    log.truncate()
    runs_data["previous_run"] = failed_tests
    runs_data["new_failures"] = diff_failed
    json.dump(runs_data, log)

def RunTestCaseOnWebsite((website, test_case, config)):
  """ Runs a |test_case| on a |website|. In case when |test_case| has
  failed it tries to rerun it. If run takes too long, then it is stopped.
  """

  profile_path = tempfile.mkdtemp()
  # The tests can be flaky. This is why we try to rerun up to 3 times.
  attempts = 3
  result = ("", "", False, "")
  logger = logging.getLogger("run_tests")
  for _ in xrange(attempts):
    shutil.rmtree(path=profile_path, ignore_errors=True)
    logger.log(SCRIPT_DEBUG, "Run of test case %s of website %s started",
               test_case, website)
    try:
      with stopit.ThreadingTimeout(seconds=100) as timeout:
        logger.log(SCRIPT_DEBUG,
                   "Run test with parameters: %s %s %s %s %s %s",
                   config.chrome_path, config.chromedriver_path,
                   profile_path, config.passwords_path,
                   website, test_case)
        result = tests.RunTest(config.chrome_path, config.chromedriver_path,
                               profile_path, config.passwords_path,
                               website, test_case)[0]
      if timeout.state != timeout.EXECUTED:
        result =  (website, test_case, False,
                   "Got %d as timeout state (see stopit.ThreadingTimeout for"
                   " the meaning of the number)" % timeout.state)
      _, _, success, _ = result
      if success:
        return result
    except Exception as e:
      result = (website, test_case, False, e)
  return result


def RunTests(config_path):
  """Runs automated tests.

  Runs the tests and returns the results through logging:
  On logging.INFO logging level, it returns the summary of how many tests
  passed and failed.
  On logging.DEBUG logging level, it returns the failure logs, if any.
  (On SCRIPT_DEBUG it returns diagnostics for this script.)

  Args:
    config_path: The path to the config INI file. See the top of the file
      for format description.
  """
  config = Config(config_path)
  logger = logging.getLogger("run_tests")
  logger.log(SCRIPT_DEBUG, "%d tests to run: %s", len(config.tests_to_run),
             config.tests_to_run)
  logger.log(SCRIPT_DEBUG, "%d test cases to run: %s",
             len(config.test_cases_to_run),
             config.test_cases_to_run)
  data = [(website, test_case, config)
          for website in config.tests_to_run
          for test_case in config.test_cases_to_run]
  number_of_processes = min([config.max_tests_in_parallel,
                             len(config.test_cases_to_run) *
                             len(config.tests_to_run)])
  p = multiprocessing.Pool(number_of_processes)
  results = p.map(RunTestCaseOnWebsite, data)
  p.close()
  p.join()
  LogResultsOfTestRun(config, results)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("config_path", metavar="N",
                      help="Path to the config.ini file.")
  args = parser.parse_args()
  RunTests(args.config_path)


if __name__ == "__main__":
  main()
