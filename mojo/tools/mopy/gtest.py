# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import Queue
import re
import subprocess
import sys
import threading
import time

from mopy.config import Config
from mopy.paths import Paths

sys.path.append(os.path.join(os.path.abspath(os.path.dirname(__file__)),
                             '..', '..', '..', 'testing'))
import xvfb


# The DISPLAY ID number used for xvfb, incremented with each use.
XVFB_DISPLAY_ID = 9


def set_color():
  '''Run gtests with color on TTY, unless its environment variable is set.'''
  if sys.stdout.isatty() and 'GTEST_COLOR' not in os.environ:
    logging.getLogger().debug('Setting GTEST_COLOR=yes')
    os.environ['GTEST_COLOR'] = 'yes'


def run_apptest(config, shell, args, apptest, isolate):
  '''Run the apptest; optionally isolating fixtures across shell invocations.

  Returns the list of tests run and the list of failures.

  Args:
    config: The mopy.config.Config for the build.
    shell: The mopy.android.AndroidShell, if Android is the target platform.
    args: The arguments for the shell or apptest.
    apptest: The application test URL.
    isolate: True if the test fixtures should be run in isolation.
  '''
  tests = [apptest]
  failed = []
  if not isolate:
    # TODO(msw): Parse fixture-granular successes and failures in this case.
    # TODO(msw): Retry fixtures that failed, not the entire apptest suite.
    if not _run_apptest_with_retry(config, shell, args, apptest):
      failed.append(apptest)
  else:
    tests = _get_fixtures(config, shell, args, apptest)
    for fixture in tests:
      arguments = args + ['--gtest_filter=%s' % fixture]
      if not _run_apptest_with_retry(config, shell, arguments, apptest):
        failed.append(fixture)
      # Abort when 20 fixtures, or a tenth of the apptest fixtures, have failed.
      # base::TestLauncher does this for timeouts and unknown results.
      if len(failed) >= max(20, len(tests) / 10):
        print 'Too many failing fixtures (%d), exiting now.' % len(failed)
        return (tests, failed + [apptest + ' aborted for excessive failures.'])
  return (tests, failed)


# TODO(msw): Determine proper test retry counts; allow configuration.
def _run_apptest_with_retry(config, shell, args, apptest, try_count=3):
  '''Runs an apptest, retrying on failure; returns True if any run passed.'''
  for try_number in range(try_count):
    if _run_apptest(config, shell, args, apptest):
      return True
    print 'Failed %s/%s test run attempts.' % (try_number + 1, try_count)
  return False


def _run_apptest(config, shell, args, apptest):
  '''Runs an apptest and checks the output for signs of gtest failure.'''
  command = _build_command_line(config, args, apptest)
  logging.getLogger().debug('Command: %s' % ' '.join(command))
  start_time = time.time()

  try:
    output = _run_test_with_xvfb(config, shell, args, apptest)
  except Exception as e:
    _print_exception(command, e)
    return False

  # Fail on output with gtest's '[  FAILED  ]' or a lack of '[  PASSED  ]'.
  # The latter condition ensures failure on broken command lines or output.
  # Check output instead of exit codes because mojo shell always exits with 0.
  if output.find('[  FAILED  ]') != -1 or output.find('[  PASSED  ]') == -1:
    _print_exception(command, output)
    return False

  ms = int(round(1000 * (time.time() - start_time)))
  logging.getLogger().debug('Passed with output (%d ms):\n%s' % (ms, output))
  return True


def _get_fixtures(config, shell, args, apptest):
  '''Returns an apptest's 'Suite.Fixture' list via --gtest_list_tests output.'''
  arguments = args + ['--gtest_list_tests']
  command = _build_command_line(config, arguments, apptest)
  logging.getLogger().debug('Command: %s' % ' '.join(command))
  try:
    tests = _run_test_with_xvfb(config, shell, arguments, apptest)
    logging.getLogger().debug('Tests for %s:\n%s' % (apptest, tests))
    # Remove log lines from the output and ensure it matches known formatting.
    tests = re.sub('^(\[|WARNING: linker:).*\n', '', tests, flags=re.MULTILINE)
    if not re.match('^(\w*\.\r?\n(  \w*\r?\n)+)+', tests):
      raise Exception('Unrecognized --gtest_list_tests output:\n%s' % tests)
    tests = tests.split('\n')
    test_list = []
    for line in tests:
      if not line:
        continue
      if line[0] != ' ':
        suite = line.strip()
        continue
      test_list.append(suite + line.strip())
    return test_list
  except Exception as e:
    _print_exception(command, e)
  return []


def _print_exception(command_line, exception):
  '''Print a formatted exception raised from a failed command execution.'''
  exit_code = ''
  if hasattr(exception, 'returncode'):
    exit_code = ' (exit code %d)' % exception.returncode
  print '\n[  FAILED  ] Command%s: %s' % (exit_code, ' '.join(command_line))
  print 72 * '-'
  if hasattr(exception, 'output'):
    print exception.output
  print str(exception)
  print 72 * '-'


def _build_command_line(config, args, apptest):
  '''Build the apptest command line. This value isn't executed on Android.'''
  not_list_tests = not '--gtest_list_tests' in args
  data_dir = ['--use-temporary-user-data-dir'] if not_list_tests else []
  return [Paths(config).mojo_runner] + data_dir + args + [apptest]


def _run_test_with_xvfb(config, shell, args, apptest):
  '''Run the test with xvfb; return the output or raise an exception.'''
  env = os.environ.copy()
  if (config.target_os != Config.OS_LINUX or '--gtest_list_tests' in args
      or not xvfb.should_start_xvfb(env)):
    return _run_test_with_timeout(config, shell, args, apptest, env)

  try:
    # Simply prepending xvfb.py to the command line precludes direct control of
    # test subprocesses, and prevents easily getting output when tests timeout.
    xvfb_proc = None
    openbox_proc = None
    global XVFB_DISPLAY_ID
    display_string = ':' + str(XVFB_DISPLAY_ID)
    (xvfb_proc, openbox_proc) = xvfb.start_xvfb(env, Paths(config).build_dir,
                                                display=display_string)
    XVFB_DISPLAY_ID = (XVFB_DISPLAY_ID + 1) % 50000
    if not xvfb_proc or not xvfb_proc.pid:
      raise Exception('Xvfb failed to start; aborting test run.')
    if not openbox_proc or not openbox_proc.pid:
      raise Exception('Openbox failed to start; aborting test run.')
    logging.getLogger().debug('Running Xvfb %s (pid %d) and Openbox (pid %d).' %
                              (display_string, xvfb_proc.pid, openbox_proc.pid))
    return _run_test_with_timeout(config, shell, args, apptest, env)
  finally:
    xvfb.kill(xvfb_proc)
    xvfb.kill(openbox_proc)


# TODO(msw): Determine proper test timeout durations (starting small).
def _run_test_with_timeout(config, shell, args, apptest, env, seconds=10):
  '''Run the test with a timeout; return the output or raise an exception.'''
  result = Queue.Queue()
  thread = threading.Thread(target=_run_test,
                            args=(config, shell, args, apptest, env, result))
  thread.start()
  process_or_shell = result.get()
  thread.join(seconds)
  timeout_exception = ''

  if thread.is_alive():
    timeout_exception = '\nError: Test timeout after %s seconds' % seconds
    logging.getLogger().debug('Killing the runner or shell for timeout.')
    try:
      process_or_shell.kill()
    except OSError:
      pass  # The process may have ended after checking |is_alive|.

  thread.join(seconds)
  if thread.is_alive():
    raise Exception('Error: Test hung and could not be killed!')
  if result.empty():
    raise Exception('Error: Test exited with no output.')
  (output, exception) = result.get()
  exception += timeout_exception
  if exception:
    raise Exception('%s%s%s' % (output, '\n' if output else '', exception))
  return output


def _run_test(config, shell, args, apptest, env, result):
  '''Run the test; put the shell/proc, output and any exception in |result|.'''
  output = ''
  exception = ''
  try:
    if config.target_os != Config.OS_ANDROID:
      command = _build_command_line(config, args, apptest)
      process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, env=env)
      result.put(process)
      (output, stderr_output) = process.communicate()
      if process.returncode:
        exception = 'Error: Test exited with code: %d\n%s' % (
            process.returncode, stderr_output)
      elif config.is_verbose:
        output += '\n' + stderr_output
    else:
      assert shell
      result.put(shell)
      (r, w) = os.pipe()
      with os.fdopen(r, 'r') as rf:
        with os.fdopen(w, 'w') as wf:
          arguments = args + [apptest]
          shell.StartActivity('MojoShellActivity', arguments, wf, wf.close)
          output = rf.read()
  except Exception as e:
    output += (e.output + '\n') if hasattr(e, 'output') else ''
    exception += str(e)
  result.put((output, exception))
