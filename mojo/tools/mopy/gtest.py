# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import sys

from mopy import test_util
from mopy.print_process_error import print_process_error


_logger = logging.getLogger()


def set_color():
  """Run gtests with color if we're on a TTY (and we're not being told
  explicitly what to do)."""
  if sys.stdout.isatty() and "GTEST_COLOR" not in os.environ:
    _logger.debug("Setting GTEST_COLOR=yes")
    os.environ["GTEST_COLOR"] = "yes"

# TODO(vtl): The return value is bizarre. Should just make it either return
# True/False, or a list of failing fixtures.
def run_fixtures(config, shell, apptest_dict, apptest, isolate, test_args,
                 shell_args):
  """Run the gtest fixtures in isolation."""

  if not isolate:
    if not RunApptestInShell(config, shell, apptest, test_args, shell_args):
      return "Failed test(s) in %r" % apptest_dict
    return "Succeeded"

  # List the apptest fixtures so they can be run independently for isolation.
  fixtures = get_fixtures(config, shell, shell_args, apptest)

  if not fixtures:
    return "Failed with no tests found."

  apptest_result = "Succeeded"
  for fixture in fixtures:
    apptest_args = test_args + ["--gtest_filter=%s" % fixture]
    success = RunApptestInShell(config, shell, apptest, apptest_args,
                                shell_args)

    if not success:
      apptest_result = "Failed test(s) in %r" % apptest_dict

  return apptest_result


def run_test(config, shell, shell_args, apps_and_args=None):
  """Runs a command line and checks the output for signs of gtest failure.

  Args:
    config: The mopy.config.Config object for the build.
    shell_args: The arguments for mojo_shell.
    apps_and_args: A Dict keyed by application URL associated to the
        application's specific arguments.
  """
  apps_and_args = apps_and_args or {}
  output = test_util.try_run_test(config, shell, shell_args, apps_and_args)
  # Fail on output with gtest's "[  FAILED  ]" or a lack of "[  PASSED  ]".
  # The latter condition ensures failure on broken command lines or output.
  # Check output instead of exit codes because mojo_shell always exits with 0.
  if (output is None or
      (output.find("[  FAILED  ]") != -1 or output.find("[  PASSED  ]") == -1)):
    print "Failed test:"
    print_process_error(
        test_util.build_command_line(config, shell_args, apps_and_args),
        output)
    return False
  _logger.debug("Succeeded with output:\n%s" % output)
  return True


def get_fixtures(config, shell, shell_args, apptest):
  """Returns the "Test.Fixture" list from an apptest using mojo_shell.

  Tests are listed by running the given apptest in mojo_shell and passing
  --gtest_list_tests. The output is parsed and reformatted into a list like
  [TestSuite.TestFixture, ... ]
  An empty list is returned on failure, with errors logged.

  Args:
    config: The mopy.config.Config object for the build.
    apptest: The URL of the test application to run.
  """
  try:
    apps_and_args = {apptest: ["--gtest_list_tests"]}
    list_output = test_util.run_test(config, shell, shell_args, apps_and_args)
    _logger.debug("Tests listed:\n%s" % list_output)
    return _gtest_list_tests(list_output)
  except Exception as e:
    print "Failed to get test fixtures:"
    print_process_error(
        test_util.build_command_line(config, shell_args, apps_and_args), e)
  return []


def _gtest_list_tests(gtest_list_tests_output):
  """Returns a list of strings formatted as TestSuite.TestFixture from the
  output of running --gtest_list_tests on a GTEST application."""

  # Remove log lines.
  gtest_list_tests_output = re.sub("^(\[|WARNING: linker:).*\n",
                                   "",
                                   gtest_list_tests_output,
                                   flags=re.MULTILINE)

  if not re.match("^(\w*\.\r?\n(  \w*\r?\n)+)+", gtest_list_tests_output):
    raise Exception("Unrecognized --gtest_list_tests output:\n%s" %
                    gtest_list_tests_output)

  output_lines = gtest_list_tests_output.split("\n")

  test_list = []
  for line in output_lines:
    if not line:
      continue
    if line[0] != " ":
      suite = line.strip()
      continue
    test_list.append(suite + line.strip())

  return test_list


def RunApptestInShell(config, shell, application, application_args, shell_args):
  return run_test(config, shell, shell_args, {application: application_args})
