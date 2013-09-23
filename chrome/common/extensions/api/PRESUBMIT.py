# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _GetJSONParseError(input_api, filename):
  try:
    contents = input_api.ReadFile(filename)
    json_comment_eater = input_api.os_path.join(
        input_api.PresubmitLocalPath(),
        '..', '..', '..', '..', 'tools',
        'json_comment_eater', 'json_comment_eater.py')
    process = input_api.subprocess.Popen(
        [input_api.python_executable, json_comment_eater],
        stdin=input_api.subprocess.PIPE,
        stdout=input_api.subprocess.PIPE,
        universal_newlines=True)
    (nommed, _) = process.communicate(input=contents)
    input_api.json.loads(nommed)
  except ValueError as e:
    return e
  return None


def _GetIDLParseError(input_api, filename):
  idl_schema = input_api.os_path.join(
      input_api.PresubmitLocalPath(),
      '..', '..', '..', '..', 'tools',
      'json_schema_compiler', 'idl_schema.py')
  process = input_api.subprocess.Popen(
      [input_api.python_executable, idl_schema, filename],
      stdout=input_api.subprocess.PIPE,
      stderr=input_api.subprocess.PIPE,
      universal_newlines=True)
  (_, error) = process.communicate()
  return error or None


def _GetParseErrors(input_api, output_api):
  # Run unit tests.
  results = []
  if input_api.AffectedFiles(
      file_filter=lambda f: 'PRESUBMIT' in f.LocalPath()):
    results = input_api.canned_checks.RunUnitTestsInDirectory(
        input_api, output_api, '.', whitelist=[r'^PRESUBMIT_test\.py$'])

  actions = {
    '.idl': _GetIDLParseError,
    '.json': _GetJSONParseError,
  }

  def get_action(affected_file):
    filename = affected_file.LocalPath()
    return actions.get(input_api.os_path.splitext(filename)[1])

  for affected_file in input_api.AffectedFiles(
      file_filter=
          lambda f: "test_presubmit" not in f.LocalPath() and get_action(f),
      include_deletes=False):
    parse_error = get_action(affected_file)(input_api,
                                            affected_file.AbsoluteLocalPath())
    if parse_error:
      results.append(output_api.PresubmitError('%s could not be parsed: %s' %
          (affected_file.LocalPath(), parse_error)))
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _GetParseErrors(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _GetParseErrors(input_api, output_api)
