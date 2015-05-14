# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import time

from mopy.config import Config
from mopy.paths import Paths
from mopy.print_process_error import print_process_error


_logger = logging.getLogger()


def build_shell_arguments(shell_args, apps_and_args=None):
  """Build the list of arguments for the shell. |shell_args| are the base
  arguments, |apps_and_args| is a dictionary that associates each application to
  its specific arguments|. Each app included will be run by the shell.
  """
  result = shell_args[:]
  if apps_and_args:
    # TODO(msw): Mojo's script uses --args-for; Chromium lacks support for that.
    for app_and_args in apps_and_args.items():
      result += app_and_args[1]
    result += apps_and_args.keys()
  return result


def get_shell_executable(config):
  paths = Paths(config=config)
  if config.target_os == Config.OS_ANDROID:
    return os.path.join(paths.src_root, "mojo", "tools",
                        "android_mojo_shell.py")
  else:
    return paths.mojo_shell_path


def build_command_line(config, shell_args, apps_and_args):
  executable = get_shell_executable(config)
  return "%s %s" % (executable, " ".join(["%r" % x for x in
                                          build_shell_arguments(
                                              shell_args, apps_and_args)]))


def run_test_android(shell, shell_args, apps_and_args):
  """Run the given test on the single/default android device."""
  assert shell
  (r, w) = os.pipe()
  with os.fdopen(r, "r") as rf:
    with os.fdopen(w, "w") as wf:
      arguments = build_shell_arguments(shell_args, apps_and_args)
      _logger.debug("Starting shell with arguments: %s" % arguments)
      start_time = time.time()
      # TODO(vtl): Do more logging in lower layers.
      shell.StartShell(arguments, wf, wf.close)
      rv = rf.read()
      run_time = time.time() - start_time
      _logger.debug("Shell completed")
      # Only log if it took more than 3 seconds.
      if run_time >= 3:
        _logger.info("Shell test took %.3f seconds; arguments: %s" %
                     (run_time, arguments))
      return rv


def run_test(config, shell, shell_args, apps_and_args):
  """Run the given test."""
  if (config.target_os == Config.OS_ANDROID):
    return run_test_android(shell, shell_args, apps_and_args)

  executable = get_shell_executable(config)
  command = ([executable] + build_shell_arguments(shell_args, apps_and_args))
  _logger.debug("Starting: %s" % " ".join(command))
  start_time = time.time()
  rv = subprocess.check_output(command, stderr=subprocess.STDOUT)
  run_time = time.time() - start_time
  _logger.debug("Completed: %s" % " ".join(command))
  # Only log if it took more than 1 second.
  if run_time >= 1:
    _logger.info("Test took %.3f seconds: %s" % (run_time, " ".join(command)))
  return rv


def try_run_test(config, shell, shell_args, apps_and_args):
  """Returns the output of a command line or an empty string on error."""
  command_line = build_command_line(config, shell_args, apps_and_args)
  _logger.debug("Running command line: %s" % command_line)
  try:
    return run_test(config, shell, shell_args, apps_and_args)
  except Exception as e:
    print_process_error(command_line, e)
  return None
