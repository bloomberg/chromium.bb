# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import subprocess
import sys

_logging = logging.getLogger()

def _print_process_error(command_line, error):
  """Properly format an exception raised from a failed command execution."""

  if command_line:
    print 'Failed command: %r' % command_line
  else:
    print 'Failed command:'
  print 72 * '-'

  if hasattr(error, 'returncode'):
    print '  with exit code %d' % error.returncode
    print 72 * '-'

  if hasattr(error, 'output'):
    print error.output
  else:
    print error
  print 72 * '-'

def set_color():
  """Run gtests with color if we're on a TTY (and we're not being told
  explicitly what to do)."""
  if sys.stdout.isatty() and 'GTEST_COLOR' not in os.environ:
    _logging.debug("Setting GTEST_COLOR=yes")
    os.environ['GTEST_COLOR'] = 'yes'


def _try_command_line(command_line):
  """Returns the output of a command line or an empty string on error."""
  _logging.debug("Running command line: %s" % command_line)
  try:
    return subprocess.check_output(command_line, stderr=subprocess.STDOUT)
  except Exception as e:
    _print_process_error(command_line, e)
  return None


def run_test(command_line):
  """Runs a command line and checks the output for signs of gtest failure."""
  output = _try_command_line(command_line)
  # Fail on output with gtest's "[  FAILED  ]" or a lack of "[  PASSED  ]".
  # The latter condition ensures failure on broken command lines or output.
  # Check output instead of exit codes because mojo_shell always exits with 0.
  if (output is None or
      (output.find("[  FAILED  ]") != -1 or output.find("[  PASSED  ]") == -1)):
    print "Failed test:"
    _print_process_error(command_line, output)
    return False
  _logging.debug("Succeeded with output:\n%s" % output)
  return True


def get_fixtures(mojo_shell, apptest):
  """Returns the "Test.Fixture" list from an apptest using mojo_shell.

  Tests are listed by running the given apptest in mojo_shell and passing
  --gtest_list_tests. The output is parsed and reformatted into a list like
  [TestSuite.TestFixture, ... ]
  An empty list is returned on failure, with errors logged.
  """
  command = [mojo_shell,
             "--args-for={0} --gtest_list_tests".format(apptest),
             apptest]
  try:
    list_output = subprocess.check_output(command, stderr=subprocess.STDOUT)
    _logging.debug("Tests listed:\n%s" % list_output)
    return _gtest_list_tests(list_output)
  except Exception as e:
    print "Failed to get test fixtures:"
    _print_process_error(command, e)
  return []


def _gtest_list_tests(gtest_list_tests_output):
  """Returns a list of strings formatted as TestSuite.TestFixture from the
  output of running --gtest_list_tests on a GTEST application."""

  # Remove log lines.
  gtest_list_tests_output = (
      re.sub("^\[.*\n", "", gtest_list_tests_output, flags=re.MULTILINE))

  if not re.match("^(\w*\.\r?\n(  \w*\r?\n)+)+", gtest_list_tests_output):
    raise Exception("Unrecognized --gtest_list_tests output:\n%s" %
                    gtest_list_tests_output)

  output_lines = gtest_list_tests_output.split('\n')

  test_list = []
  for line in output_lines:
    if not line:
      continue
    if line[0] != ' ':
      suite = line.strip()
      continue
    test_list.append(suite + line.strip())

  return test_list
