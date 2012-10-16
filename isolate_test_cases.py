#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Traces each test cases of a google-test executable individually and generates
or updates an .isolate file.
"""

import os
import subprocess
import sys

import isolate
import run_test_cases
import trace_inputs
import trace_test_cases

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))


def isolate_test_cases(
    cmd, cwd_dir, test_cases, jobs, _timeout, isolated_file, isolate_file,
    root_dir, variables):
  assert os.path.isabs(root_dir) and os.path.isdir(root_dir), root_dir

  logname = isolated_file + '.log'
  basename = isolated_file.rsplit('.', 1)[0]

  # Do the actual tracing.
  results = trace_test_cases.trace_test_cases(
      cmd, cwd_dir, test_cases, jobs, logname)

  api = trace_inputs.get_api()
  logs = dict(
      (i.pop('trace'), i)
      for i in api.parse_log(logname, isolate.default_blacklist))
  exception = None
  try:
    inputs = []
    for items in results:
      item = items[-1]
      assert item['valid']
      # Load the results;
      test_case = item['test_case']
      if not test_case in logs:
        assert False
      log_dict = logs[test_case]
      if log_dict.get('exception'):
        exception = exception or log_dict['exception']
        continue
      files = log_dict['results'].strip_root(root_dir).files
      tracked, touched = isolate.split_touched(files)
      value = isolate.generate_isolate(
          tracked,
          [],
          touched,
          root_dir,
          variables,
          cwd_dir)

      with open(basename + '.' + test_case + '.isolate', 'w') as f:
        isolate.pretty_print(value, f)
      test_cases.append(inputs)

    # Merges back. Note that it is possible to blow up the command line
    # argument length here but writing the files is still useful. Convert to
    # importing the module instead if necessary.
    merge_cmd = ['isolate_merge.py', isolate_file] + inputs
    merge_cmd.extend(['-o', isolate_file])
    proc = subprocess.Popen(run_test_cases.fix_python_path(merge_cmd))
    proc.communicate()
    return proc.returncode
  finally:
    if exception:
      raise exception[0], exception[1], exception[2]


def main():
  """CLI frontend to validate arguments."""
  parser = run_test_cases.OptionParserTestCases(
      usage='%prog <options> -r <.isolated>')
  parser.format_description = lambda *_: parser.description
  isolate.add_variable_option(parser)
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported arg: %s' % args)
  isolate.parse_variable_option(parser, options, True)

  try:
    config = isolate.CompleteState.load_files(options.result)
    reldir = os.path.join(config.root_dir, config.isolated.relative_cwd)
    command = run_test_cases.fix_python_path(config.isolated.command)
    test_cases = parser.process_gtest_options(command, reldir, options)
    if not test_cases:
      print >> sys.stderr, 'No test case to run'
      return 1
    return isolate_test_cases(
        command,
        reldir,
        test_cases,
        options.jobs,
        options.timeout,
        config.isolated_filepath,
        config.saved_state.isolate_file,
        config.root_dir,
        options.variables)
  except isolate.ExecutionError, e:
    print >> sys.stderr, str(e)
    return 1


if __name__ == '__main__':
  sys.exit(main())
