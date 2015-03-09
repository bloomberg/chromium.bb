# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This file allows the bots to be easily configured and run the tests.

Running this script requires passing --config-path with a path to a config file
of the following structure:

  [data_files]
  passwords_path=<path to a file with passwords>
  [binaries]
  chrome-path=<chrome binary path>
  chromedriver-path=<chrome driver path>
  [run_options]
  # |write_to_sheet| is optional, the default value is false.
  write_to_sheet=[false|true]
  # |tests_in_parallel| is optional, the default value is 1.
  tests_in_parallel=<number of parallel tests>
  # |tests_to_runs| field is optional, if it is absent all tests will be run.
  tests_to_run=<test names to run, comma delimited>
  [output]
  # |save-path| is optional, the default value is /dev/null.
  save-path=<file where to save result>
  [sheet_info]
  # This section is required only when write_to_sheet=true
  pkey=full_path
  client_email=email_assigned_by_google_dev_console
  sheet_key=sheet_key_from_sheet_url
"""
from datetime import datetime
import ConfigParser
import sys
import httplib2
import os
import shutil
import subprocess
import tempfile
import time
sheet_libraries_import_error = None
try:
# TODO(vabr) Remove this dependency http://crbug.com/418485#c4.
  from Sheet import Sheet
  from apiclient.discovery import build
  from gdata.gauth import OAuth2TokenFromCredentials
  from gdata.spreadsheet.service import SpreadsheetsService
  from oauth2client.client import SignedJwtAssertionCredentials
  import oauth2client.tools
except ImportError as err:
  sheet_libraries_import_error = err


from environment import Environment
import tests

_CREDENTIAL_SCOPES = "https://spreadsheets.google.com/feeds"

# TODO(dvadym) Change all prints in this file to correspond logging.

# TODO(dvadym) Consider to move this class to separate file.
class SheetWriter(object):

  def __init__(self, config):
    self.write_to_sheet = config.getboolean("run_options", "write_to_sheet")
    if not self.write_to_sheet:
      return
    if sheet_libraries_import_error:
      raise sheet_libraries_import_error
    self.pkey = config.get("sheet_info", "pkey")
    self.client_email = config.get("sheet_info", "client_email")
    self.sheet_key = config.get("sheet_info", "sheet_key")
    _, self.access_token = self._authenticate()
    self.sheet = self._spredsheeet_for_logging()

  # TODO(melandory): Function _authenticate belongs to separate module.
  def _authenticate(self):
    http, token = None, None
    with open(self.pkey) as pkey_file:
      private_key = pkey_file.read()
      credentials = SignedJwtAssertionCredentials(
          self.client_email, private_key, _CREDENTIAL_SCOPES)
      http = httplib2.Http()
      http = credentials.authorize(http)
      build("drive", "v2", http=http)
      token = OAuth2TokenFromCredentials(credentials).access_token
    return http, token

  # TODO(melandory): Functionality of _spredsheeet_for_logging belongs
  # to websitetests, because this way we do not need to write results of run
  # in separate file and then read it here.
  def _spredsheeet_for_logging(self):
    """ Connects to document where result of test run will be logged. """
    # Connect to trix
    service = SpreadsheetsService(additional_headers={
              "Authorization": "Bearer " + self.access_token})
    sheet = Sheet(service, self.sheet_key)
    return sheet

  def write_line_to_sheet(self, data):
    if not self.write_to_sheet:
      return
    try:
      self.sheet.InsertRow(self.sheet.row_count, data)
    except Exception:
      pass  # TODO(melandory): Sometimes writing to spreadsheet fails. We need
            # to deal with it better that just ignoring it.

class TestRunner(object):

  def __init__(self, general_test_cmd, test_name):
    """ Args:
     general_test_cmd: String contains part of run command common for all tests,
      [2] is placeholder for test name.
     test_name: Test name (facebook for example).
    """
    self.profile_path = tempfile.mkdtemp()
    results = tempfile.NamedTemporaryFile(delete=False)
    self.results_path = results.name
    results.close()
    self.test_cmd = general_test_cmd + ["--profile-path", self.profile_path,
                                        "--save-path", self.results_path]
    self.test_cmd[2] = self.test_name = test_name
    # TODO(rchtara): Using "timeout is just temporary until a better,
    # platform-independent solution is found.
    # The website test runs in two passes, each pass has an internal
    # timeout of 200s for waiting (see |remaining_time_to_wait| and
    # Wait() in websitetest.py). Accounting for some more time spent on
    # the non-waiting execution, 300 seconds should be the upper bound on
    # the runtime of one pass, thus 600 seconds for the whole test.
    self.test_cmd = ["timeout", "600"] + self.test_cmd
    self.runner_process = None
    # The tests can be flaky. This is why we try to rerun up to 3 times.
    self.max_test_runs_left = 3
    self.failures = []
    self._run_test()

  def get_test_result(self):
    """ Return None if result is not ready yet."""
    test_running = self.runner_process and self.runner_process.poll() is None
    if test_running: return None
    # Test is not running, now we have to check if we want to start it again.
    if self._check_if_test_passed():
      print "Test " + self.test_name + " passed"
      return "pass", []
    if self.max_test_runs_left == 0:
      print "Test " + self.test_name + " failed"
      return "fail", self.failures
    self._run_test()
    return None

  def _check_if_test_passed(self):
    if os.path.isfile(self.results_path):
      results = open(self.results_path, "r")
      count = 0  # Count the number of successful tests.
      for line in results:
        # TODO(melandory): We do not need to send all this data to sheet.
        self.failures.append(line)
        count += line.count("successful='True'")
      results.close()
      # There is only two tests running for every website: the prompt and
      # the normal test. If both of the tests were successful, the tests
      # would be stopped for the current website.
      print "Test run of %s %s" % (self.test_name, "passed"
                                   if count == 2 else "failed")
      if count == 2:
        return True
    return False

  def _run_test(self):
    """Run separate process that once run test for one site."""
    try:
      os.remove(self.results_path)
    except Exception:
      pass
    try:
      shutil.rmtree(self.profile_path)
    except Exception:
      pass
    self.max_test_runs_left -= 1
    print "Run of test %s started" % self.test_name
    self.runner_process = subprocess.Popen(self.test_cmd)

def _apply_defaults(config, defaults):
  """Adds default values from |defaults| to |config|.

  Note: This differs from ConfigParser's mechanism for providing defaults in
  two aspects:
    * The "defaults" here become explicit, and are associated with sections.
    * Sections get created for the added defaults where needed, that is, if
      they do not exist before.

  Args:
    config: A ConfigParser instance to be updated
    defaults: A dictionary mapping (section_string, option_string) pairs
      to string values. For every section/option combination not already
      contained in |config|, the value from |defaults| is stored in |config|.
  """
  for (section, option) in defaults:
    if not config.has_section(section):
      config.add_section(section)
    if not config.has_option(section, option):
      config.set(section, option, defaults[(section, option)])

def run_tests(config_path):
  """ Runs automated tests. """
  environment = Environment("", "", "", None, False)
  defaults = { ("output", "save-path"): "/dev/null",
               ("run_options", "tests_in_parallel"): "1",
               ("run_options", "write_to_sheet"): "false" }
  config = ConfigParser.ConfigParser()
  _apply_defaults(config, defaults)
  config.read(config_path)
  date = datetime.now().strftime('%Y-%m-%dT%H:%M:%S')
  max_tests_in_parallel = config.getint("run_options", "tests_in_parallel")
  sheet_writer = SheetWriter(config)
  full_path = os.path.realpath(__file__)
  tests_dir = os.path.dirname(full_path)
  tests_path = os.path.join(tests_dir, "tests.py")
  general_test_cmd =  ["python", tests_path, "test_name_placeholder",
     "--chrome-path", config.get("binaries", "chrome-path"),
     "--chromedriver-path", config.get("binaries", "chromedriver-path"),
     "--passwords-path", config.get("data_files", "passwords_path")]
  runners = []
  if config.has_option("run_options", "tests_to_run"):
    user_selected_tests = config.get("run_options", "tests_to_run").split(',')
    tests_to_run = user_selected_tests
  else:
    tests.Tests(environment)
    tests_to_run = [test.name for test in environment.websitetests]

  with open(config.get("output", "save-path"), 'w') as savefile:
    print "Tests to run %d\nTests: %s" % (len(tests_to_run), tests_to_run)
    while len(runners) + len(tests_to_run) > 0:
      i = 0
      while i < len(runners):
        result = runners[i].get_test_result()
        if result:  # This test run is finished.
          status, log = result
          testinfo = [runners[i].test_name, status, date, " | ".join(log)]
          sheet_writer.write_line_to_sheet(testinfo)
          print>>savefile, " ".join(testinfo)
          del runners[i]
        else:
          i += 1
      while len(runners) < max_tests_in_parallel and len(tests_to_run) > 0:
        runners.append(TestRunner(general_test_cmd, tests_to_run.pop()))
      time.sleep(1)  # Let us wait for worker process to finish.

if __name__ == "__main__":
  if len(sys.argv) != 2:
    print "Synopsis:\n python run_tests.py <config_path>"
  config_path = sys.argv[1]
  run_tests(config_path)
