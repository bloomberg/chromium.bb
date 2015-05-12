#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A test runner for gtest application tests."""

import argparse
import logging
import sys

from mopy import dart_apptest
from mopy import gtest
from mopy.android import AndroidShell
from mopy.config import Config
from mopy.gn import ConfigForGNArgs, ParseGNConfig
from mopy.log import InitLogging
from mopy.paths import Paths


_logger = logging.getLogger()


def main():
  parser = argparse.ArgumentParser(description="A test runner for application "
                                               "tests.")

  parser.add_argument("--verbose", help="be verbose (multiple times for more)",
                      default=0, dest="verbose_count", action="count")
  parser.add_argument("test_list_file", type=file,
                      help="a file listing apptests to run")
  parser.add_argument("build_dir", type=str,
                      help="the build output directory")
  args = parser.parse_args()

  InitLogging(args.verbose_count)
  config = ConfigForGNArgs(ParseGNConfig(args.build_dir))

  _logger.debug("Test list file: %s", args.test_list_file)
  execution_globals = {"config": config}
  exec args.test_list_file in execution_globals
  test_list = execution_globals["tests"]
  _logger.debug("Test list: %s" % test_list)

  extra_args = []
  if config.target_os == Config.OS_ANDROID:
    paths = Paths(config)
    shell = AndroidShell(paths.target_mojo_shell_path, paths.build_dir,
                         paths.adb_path)
    extra_args.extend(shell.PrepareShellRun('localhost'))
  else:
    shell = None

  gtest.set_color()

  exit_code = 0
  for test_dict in test_list:
    test = test_dict["test"]
    test_name = test_dict.get("name", test)
    test_type = test_dict.get("type", "gtest")
    test_args = test_dict.get("test-args", [])
    shell_args = test_dict.get("shell-args", []) + extra_args

    _logger.info("Will start: %s" % test_name)
    print "Running %s...." % test_name,
    sys.stdout.flush()

    if test_type == "dart":
      apptest_result = dart_apptest.run_test(config, shell, test_dict,
                                             shell_args, {test: test_args})
    elif test_type == "gtest":
      apptest_result = gtest.run_fixtures(config, shell, test_dict,
                                          test, False,
                                          test_args, shell_args)
    elif test_type == "gtest_isolated":
      apptest_result = gtest.run_fixtures(config, shell, test_dict,
                                          test, True, test_args, shell_args)
    else:
      apptest_result = "Invalid test type in %r" % test_dict

    if apptest_result != "Succeeded":
      exit_code = 1
    print apptest_result
    _logger.info("Completed: %s" % test_name)

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
