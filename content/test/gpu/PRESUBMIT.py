# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _PyLintChecks(input_api, output_api):
  pylint_checks = input_api.canned_checks.GetPylint(input_api, output_api,
          extra_paths_list=_GetPathsToPrepend(input_api), pylintrc='pylintrc')
  return input_api.RunTests(pylint_checks)

def _GetPathsToPrepend(input_api):
  current_dir = input_api.PresubmitLocalPath()
  chromium_src_dir = input_api.os_path.join(current_dir, '..', '..', '..')
  return [
    input_api.os_path.join(current_dir, 'gpu_tests'),
    input_api.os_path.join(chromium_src_dir, 'tools', 'perf'),
    input_api.os_path.join(chromium_src_dir,
        'third_party', 'catapult', 'telemetry'),
    input_api.os_path.join(chromium_src_dir,
        'third_party', 'catapult', 'catapult_base'),
  ]

def _WebGLTextExpectationsTests(input_api, output_api):
  if not input_api.AffectedFiles():
    return []

  cmd = [
    input_api.os_path.join(input_api.PresubmitLocalPath(), 'run_unittests.py'),
    'gpu_tests',
  ]
  if input_api.platform == 'win32':
    cmd = [input_api.python_executable] + cmd

  if input_api.is_committing:
    message_type = output_api.PresubmitError
  else:
    message_type = output_api.PresubmitPromptWarning

  cmd_name = 'run_content_test_gpu_unittests',
  test_cmd = input_api.Command(
    name=cmd_name,
    cmd=cmd,
    kwargs={},
    message=message_type,
  )

  if input_api.verbose:
    print 'Running', cmd_name
  return input_api.RunTests([test_cmd])

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_PyLintChecks(input_api, output_api))
  results.extend(_WebGLTextExpectationsTests(input_api, output_api))
  return results

def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_PyLintChecks(input_api, output_api))
  results.extend(_WebGLTextExpectationsTests(input_api, output_api))
  return results
