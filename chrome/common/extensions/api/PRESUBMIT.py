# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _GetJSONParseError(input_api, contents):
  try:
    json_comment_eater = input_api.os_path.join(
        input_api.PresubmitLocalPath(),
        '..', '..', '..', '..', 'tools',
        'json_comment_eater', 'json_comment_eater.py')
    process = input_api.subprocess.Popen(
        [input_api.python_executable, json_comment_eater],
        stdin=input_api.subprocess.PIPE,
        stdout=input_api.subprocess.PIPE)
    (nommed, _) = process.communicate(input=contents)
    input_api.json.loads(nommed)
  except ValueError as e:
    return e
  return None


def _GetParseErrors(input_api, output_api):
  # Run unit tests.
  results = []
  if input_api.AffectedFiles(
      file_filter=lambda f: 'PRESUBMIT' in f.LocalPath()):
    results = input_api.canned_checks.RunUnitTestsInDirectory(
        input_api, output_api, '.', whitelist=[r'^PRESUBMIT_test\.py$'])

  for affected_file in input_api.AffectedFiles(
      file_filter=lambda f: f.LocalPath().endswith('.json')):
    filename = affected_file.AbsoluteLocalPath()
    contents = input_api.ReadFile(filename)
    parse_error = _GetJSONParseError(input_api, contents)
    if parse_error:
      results.append(output_api.PresubmitError(
          'Features file %s could not be parsed: %s' %
          (affected_file.LocalPath(), parse_error)))
    # TODO(yoz): Also ensure IDL files are parseable.
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _GetParseErrors(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _GetParseErrors(input_api, output_api)
