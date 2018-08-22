# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for android_webview/javatests/

Runs various style checks before upload.
"""

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckAwJUnitTestRunner(input_api, output_api))
  return results

def _CheckAwJUnitTestRunner(input_api, output_api):
  """Checks that new tests use the AwJUnit4ClassRunner instead of some other
  test runner. This is because WebView has special logic in the
  AwJUnit4ClassRunner.
  """

  run_with_pattern = input_api.re.compile(
      r'^@RunWith\((.*)\)$')
  correct_runner = 'AwJUnit4ClassRunner.class'

  errors = []

  def _FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        black_list=input_api.DEFAULT_BLACK_LIST,
        white_list=[r'.*\.java$'])

  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_num, line in f.ChangedContents():
      match = run_with_pattern.search(line)
      if match and match.group(1) != correct_runner:
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitPromptWarning("""
android_webview/javatests/ should use the AwJUnit4ClassRunner test runner, not
any other test runner (e.g., BaseJUnit4ClassRunner).
""",
        errors))

  return results
