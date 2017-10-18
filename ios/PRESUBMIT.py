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

def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds an extra try bot to the CL description in order to run Cronet
  tests in addition to CQ try bots.
  """

  # TODO(crbug.com/712733): Remove this once Cronet bots are deployed on CQ.
  try_bots = ['master.tryserver.chromium.mac:ios-simulator-cronet']

  return output_api.EnsureCQIncludeTrybotsAreAdded(
    cl, try_bots, 'Automatically added Cronet trybots to run tests on CQ.')
