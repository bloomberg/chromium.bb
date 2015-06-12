# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import subprocess
import sys
import time

from mopy.config import Config
from mopy.paths import Paths


def set_color():
  """Run gtests with color on TTY, unless its environment variable is set."""
  if sys.stdout.isatty() and "GTEST_COLOR" not in os.environ:
    logging.getLogger().debug("Setting GTEST_COLOR=yes")
    os.environ["GTEST_COLOR"] = "yes"


def run_apptest(config, shell, args, apptest, isolate):
  """Run the apptest; optionally isolating fixtures across shell invocations.

  Args:
    config: The mopy.config.Config for the build.
    shell: The mopy.android.AndroidShell, if Android is the target platform.
    args: The arguments for the shell or apptest.
    apptest: The application test URL.
    isolate: True if the test fixtures should be run in isolation.
  """
  if not isolate:
    return _run_apptest(config, shell, args, apptest)

  fixtures = _get_fixtures(config, shell, args, apptest)
  result = True if fixtures else False
  for fixture in fixtures:
    arguments = args + ["--gtest_filter=%s" % fixture]
    if not _run_apptest(config, shell, arguments, apptest):
      result = False
  return result


def _run_apptest(config, shell, args, apptest):
  """Runs an apptest and checks the output for signs of gtest failure."""
  command = _build_command_line(config, args, apptest)
  logging.getLogger().debug("Command: %s" % " ".join(command))
  start_time = time.time()

  try:
    output = _run_test(config, shell, args, apptest)
  except Exception as e:
    _print_error(command, e)
    return False

  # Fail on output with gtest's "[  FAILED  ]" or a lack of "[  PASSED  ]".
  # The latter condition ensures failure on broken command lines or output.
  # Check output instead of exit codes because mojo shell always exits with 0.
  if output.find("[  FAILED  ]") != -1 or output.find("[  PASSED  ]") == -1:
    _print_error(command, output)
    return False

  ms = int(round(1000 * (time.time() - start_time)))
  logging.getLogger().debug("Passed with output (%d ms):\n%s" % (ms, output))
  return True


def _get_fixtures(config, shell, args, apptest):
  """Returns an apptest's "Suite.Fixture" list via --gtest_list_tests output."""
  try:
    arguments = args + ["--gtest_list_tests"]
    tests = _run_test(config, shell, arguments, apptest)
    logging.getLogger().debug("Tests for %s:\n%s" % (apptest, tests))
    # Remove log lines from the output and ensure it matches known formatting.
    tests = re.sub("^(\[|WARNING: linker:).*\n", "", tests, flags=re.MULTILINE)
    if not re.match("^(\w*\.\r?\n(  \w*\r?\n)+)+", tests):
      raise Exception("Unrecognized --gtest_list_tests output:\n%s" % tests)
    tests = tests.split("\n")
    test_list = []
    for line in tests:
      if not line:
        continue
      if line[0] != " ":
        suite = line.strip()
        continue
      test_list.append(suite + line.strip())
    return test_list
  except Exception as e:
    _print_error(_build_command_line(config, arguments, apptest), e)
  return []


def _print_error(command_line, error):
  """Properly format an exception raised from a failed command execution."""
  exit_code = ""
  if hasattr(error, 'returncode'):
    exit_code = " (exit code %d)" % error.returncode
  print "\n[  FAILED  ] Command%s: %s" % (exit_code, " ".join(command_line))
  print 72 * "-"
  print error.output if hasattr(error, 'output') else error
  print 72 * "-"


def _build_command_line(config, args, apptest):
  """Build the apptest command line. This value isn't executed on Android."""
  return [Paths(config).mojo_runner] + args + [apptest]


def _run_test(config, shell, args, apptest):
  """Run the given test and return the output."""
  if (config.target_os != Config.OS_ANDROID):
    command = _build_command_line(config, args, apptest)
    return subprocess.check_output(command, stderr=subprocess.STDOUT)

  assert shell
  (r, w) = os.pipe()
  with os.fdopen(r, "r") as rf:
    with os.fdopen(w, "w") as wf:
      shell.StartActivity('MojoShellActivity', args + [apptest], wf, wf.close)
      return rf.read()
