# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ash (Chrome OS system UI).

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


# Tuples of (func_name, message, excluded_paths).
_BANNED_FUNCTIONS = (
    (
      r'aura::Env::GetInstance',
      (
        'aura::Env::GetInstance is banned in //ash because it will return the',
        'wrong aura::Env in SingleProcessMash mode. See //ash/README.md.',
        'Use ash::Shell::Get()->aura_env() instead.'
      ),
      (
        # Components are mini-apps that run in their own process and only have
        # a single aura::Env.
        r'^ash/components/',

        # ash_shell is a separate test binary.
        r'^ash/shell/',
      )
    ),
)


def _CheckBannedFunctions(input_api, output_api):
  """Make sure that banned functions are not used."""
  errors = []

  def IsExcludedFile(affected_file, excluded_paths):
    local_path = affected_file.LocalPath()
    for item in excluded_paths:
      if input_api.re.match(item, local_path):
        return True
    return False

  def CheckForMatch(affected_file, line_num, line, func_name, message):
    if func_name in line:
      errors.append('    %s:%d:' % (affected_file.LocalPath(), line_num))
      for message_line in message:
        errors.append('      %s' % message_line)

  file_filter = lambda f: f.LocalPath().endswith(('.cc', '.h'))
  for f in input_api.AffectedFiles(file_filter=file_filter):
    for line_num, line in f.ChangedContents():
      for func_name, message, excluded_paths in _BANNED_FUNCTIONS:
        if IsExcludedFile(f, excluded_paths):
          continue
        CheckForMatch(f, line_num, line, func_name, message)

  result = []
  if (errors):
    result.append(output_api.PresubmitError(
        'Banned functions were used.\n' + '\n'.join(errors)))
  return result


def CheckChange(input_api, output_api):
  results = []
  results += _CheckBannedFunctions(input_api, output_api)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
