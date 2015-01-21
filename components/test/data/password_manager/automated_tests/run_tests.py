# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This file allows the bots to be easily configure and run the tests."""

import argparse
import os
import tempfile

from environment import Environment
import tests

def run_tests(
    save_path, chrome_path, chromedriver_path,
    passwords_path, profile_path, *args, **kwargs):
  """ Runs automated tests.

  Args:
   save_path: File, where results of run will be logged.
   chrome_path: Location of Chrome binary.
   chromedriver_path: Location of Chrome Driver.
   passwords_path: Location of file with credentials.
   profile_path: Location of profile path.
   *args: Variable length argument list.
   **kwargs: Arbitrary keyword arguments.
  """
  environment = Environment('', '', '', None, False)
  tests.Tests(environment)

  xml = open(save_path, "w")
  xml.write("<xml>")
  try:
    results = tempfile.NamedTemporaryFile(
        dir=os.path.join(tempfile.gettempdir()), delete=False)
    results_path = results.name
    results.close()

    full_path = os.path.realpath(__file__)
    tests_dir = os.path.dirname(full_path)
    tests_path = os.path.join(tests_dir, "tests.py")

    for websitetest in environment.websitetests:
      # The tests can be flaky. This is why we try to rerun up to 3 times.
      for x in range(0, 3):
        # TODO(rchtara): Using "pkill" is just temporary until a better,
        # platform-independent solution is found.
        os.system("pkill chrome")
        try:
          os.remove(results_path)
        except Exception:
          pass
        # TODO(rchtara): Using "timeout is just temporary until a better,
        # platform-independent solution is found.

        # The website test runs in two passes, each pass has an internal
        # timeout of 200s for waiting (see |remaining_time_to_wait| and
        # Wait() in websitetest.py). Accounting for some more time spent on
        # the non-waiting execution, 300 seconds should be the upper bound on
        # the runtime of one pass, thus 600 seconds for the whole test.
        os.system("timeout 600 python %s %s --chrome-path %s "
            "--chromedriver-path %s --passwords-path %s --profile-path %s "
            "--save-path %s" %
            (tests_path, websitetest.name, chrome_path,
             chromedriver_path, passwords_path,
             profile_path, results_path))
        if os.path.isfile(results_path):
          results = open(results_path, "r")
          count = 0 # Count the number of successful tests.
          for line in results:
            xml.write(line)
            count += line.count("successful='True'")
          results.close()
          # There is only two tests running for every website: the prompt and
          # the normal test. If both of the tests were successful, the tests
          # would be stopped for the current website.
          if count == 2:
            break
        else:
          xml.write("<result><test name='%s' type='prompt' successful='false'>"
              "</test><test name='%s' type='normal' successful='false'></test>"
              "</result>" % (websitetest.name, websitetest.name))
  finally:
    try:
      os.remove(results_path)
    except Exception:
      pass

  xml.write("</xml>")
  xml.close()

if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Password Manager automated tests runner help.")
  parser.add_argument(
      "--chrome-path", action="store", dest="chrome_path",
      help="Set the chrome path (required).", required=True)
  parser.add_argument(
      "--chromedriver-path", action="store", dest="chromedriver_path",
      help="Set the chromedriver path (required).", required=True)
  parser.add_argument(
      "--profile-path", action="store", dest="profile_path",
      help="Set the profile path (required). You just need to choose a "
           "temporary empty folder. If the folder is not empty all its content "
           "is going to be removed.",
      required=True)
  parser.add_argument(
      "--passwords-path", action="store", dest="passwords_path",
      help="Set the usernames/passwords path (required).", required=True)
  parser.add_argument("--save-path", action="store",  dest="save_path",
      help="Write the results in a file.", required=True)
  args = vars(parser.parse_args())
  run_tests(**args)
