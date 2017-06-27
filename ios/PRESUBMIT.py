# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ios.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

TODO_PATTERN = r'TO[D]O\(([^\)]*)\)'
CRBUG_PATTERN = r'crbug\.com/\d+$'

def _CheckBugInToDo(input_api, output_api):
  """ Checks whether TODOs in ios code are identified by a bug number."""
  todo_regex = input_api.re.compile(TODO_PATTERN)
  crbug_regex = input_api.re.compile(CRBUG_PATTERN)

  errors = []
  for f in input_api.AffectedFiles():
    for line_num, line in f.ChangedContents():
      todo_match = todo_regex.search(line)
      if not todo_match:
        continue
      crbug_match = crbug_regex.match(todo_match.group(1))
      if not crbug_match:
        errors.append('%s:%s' % (f.LocalPath(), line_num))
  if not errors:
    return []

  plural_suffix = '' if len(errors) == 1 else 's'
  error_message = '\n'.join([
      'Found TO''DO%(plural)s without bug number%(plural)s (expected format is '
      '\"TO''DO(crbug.com/######)\":' % {'plural': plural_suffix}
  ] + errors) + '\n'

  return [output_api.PresubmitError(error_message)]


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckBugInToDo(input_api, output_api))
  return results
