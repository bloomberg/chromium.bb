#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Traces each test cases of a google-test executable individually and generates
or updates an .isolate file.

If the trace hangs up, you can still take advantage of the traces up to the
points it got to by Ctrl-C'ing out, then running isolate.py merge -r
out/release/foo_test.isolated. the reason it works is because both isolate.py
and isolate_test_cases.py uses the same filename for the trace by default.
"""

import logging
import os
import subprocess
import sys

import isolate
import run_test_cases
import trace_inputs
import trace_test_cases

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def isolate_test_cases(
    cmd, test_cases, jobs, isolated_file, isolate_file,
    root_dir, reldir, variables):
  assert os.path.isabs(root_dir) and os.path.isdir(root_dir), root_dir

  logname = isolated_file + '.log'
  basename = isolated_file.rsplit('.', 1)[0]
  cwd_dir = os.path.join(root_dir, reldir)
  # Do the actual tracing.
  results = trace_test_cases.trace_test_cases(
      cmd, cwd_dir, test_cases, jobs, logname)

  api = trace_inputs.get_api()
  logs = dict(
      (i.pop('trace'), i)
      for i in api.parse_log(logname, isolate.chromium_default_blacklist, None))
  exception = None
  try:
    inputs = []
    for items in results:
      item = items[-1]
      assert item['valid']
      # Load the results;
      log_dict = logs[item['tracename']]
      if log_dict.get('exception'):
        exception = exception or log_dict['exception']
        logging.error('Got exception: %s', exception)
        continue
      files = log_dict['results'].strip_root(root_dir).files
      tracked, touched = isolate.split_touched(files)
      value = isolate.generate_isolate(
          tracked,
          [],
          touched,
          root_dir,
          variables,
          reldir)
      # item['test_case'] could be an invalid file name.
      out = basename + '.' + item['tracename'] + '.isolate'
      with open(out, 'w') as f:
        isolate.pretty_print(value, f)
      inputs.append(out)

    # Merges back. Note that it is possible to blow up the command line
    # argument length here but writing the files is still useful. Convert to
    # importing the module instead if necessary.
    merge_cmd = [
      sys.executable,
      os.path.join(BASE_DIR, 'isolate_merge.py'),
      isolate_file,
      '-o', isolate_file,
    ]
    merge_cmd.extend(inputs)
    logging.info(merge_cmd)
    proc = subprocess.Popen(merge_cmd)
    proc.communicate()
    return proc.returncode
  finally:
    if exception:
      raise exception[0], exception[1], exception[2]


def test_xvfb(command, rel_dir):
  """Calls back ourself if not running inside Xvfb and it's on the command line
  to run.

  Otherwise the X session will die while trying to start too many Xvfb
  instances.
  """
  if os.environ.get('_CHROMIUM_INSIDE_XVFB') == '1':
    return
  for index, item in enumerate(command):
    if item.endswith('xvfb.py'):
      # Note this has inside knowledge about src/testing/xvfb.py.
      print('Restarting itself under Xvfb')
      prefix = command[index:index+2]
      prefix[0] = os.path.normpath(os.path.join(rel_dir, prefix[0]))
      prefix[1] = os.path.normpath(os.path.join(rel_dir, prefix[1]))
      cmd = run_test_cases.fix_python_path(prefix + sys.argv)
      sys.exit(subprocess.call(cmd))


def safely_load_isolated(parser, options):
  config = isolate.CompleteState.load_files(options.isolated)
  reldir = os.path.join(config.root_dir, config.saved_state.relative_cwd)
  command = config.saved_state.command
  test_cases = []
  if command:
    command = run_test_cases.fix_python_path(command)
    test_xvfb(command, reldir)
    test_cases = parser.process_gtest_options(command, reldir, options)
  return config, command, test_cases


def main():
  """CLI frontend to validate arguments."""
  run_test_cases.run_isolated.disable_buffering()
  parser = run_test_cases.OptionParserTestCases(
      usage='%prog <options> --isolated <.isolated>')
  parser.format_description = lambda *_: parser.description
  isolate.add_variable_option(parser)
  # TODO(maruel): Add support for options.timeout.
  parser.remove_option('--timeout')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported arg: %s' % args)
  isolate.parse_isolated_option(parser, options, os.getcwd(), True)
  isolate.parse_variable_option(options)

  try:
    config, command, test_cases = safely_load_isolated(parser, options)
    if not command:
      parser.error('A command must be defined')
    if not test_cases:
      parser.error('No test case to run')

    config.saved_state.variables.update(options.variables)
    return isolate_test_cases(
        command,
        test_cases,
        options.jobs,
        config.isolated_filepath,
        config.saved_state.isolate_file,
        config.root_dir,
        config.saved_state.relative_cwd,
        config.saved_state.variables)
  except isolate.ExecutionError, e:
    print >> sys.stderr, str(e)
    return 1


if __name__ == '__main__':
  sys.exit(main())
