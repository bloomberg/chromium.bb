# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for content/test/gpu.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def _GetPathsToPrepend(input_api):
  current_dir = input_api.PresubmitLocalPath()
  chromium_src_dir = input_api.os_path.join(current_dir, '..', '..', '..')
  return [
    input_api.os_path.join(current_dir, 'gpu_tests'),
    input_api.os_path.join(chromium_src_dir, 'tools', 'perf'),
    input_api.os_path.join(chromium_src_dir,
        'third_party', 'catapult', 'telemetry'),
    input_api.os_path.join(chromium_src_dir,
        'third_party', 'catapult', 'common', 'py_utils'),
  ]

def _GpuUnittestsArePassingCheck(input_api, output_api):
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
  results.extend(_GpuUnittestsArePassingCheck(input_api, output_api))
  return results

def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_GpuUnittestsArePassingCheck(input_api, output_api))
  return results

def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook modifies the CL description in order to run extra GPU
  tests (in particular, the WebGL 2.0 conformance tests) in addition
  to the regular CQ try bots. This test suite is too large to run
  against all Chromium commits, but should be run against changes
  likely to affect these tests.
  """
  return output_api.EnsureCQIncludeTrybotsAreAdded(
    cl,
    [
      'master.tryserver.chromium.linux:linux_optional_gpu_tests_rel',
      'master.tryserver.chromium.mac:mac_optional_gpu_tests_rel',
      'master.tryserver.chromium.win:win_optional_gpu_tests_rel',
      'master.tryserver.chromium.android:android_optional_gpu_tests_rel',
    ],
    'Automatically added optional GPU tests to run on CQ.')
