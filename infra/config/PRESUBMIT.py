# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _CommonChecks(input_api, output_api):
  commands = []
  touches_cq = False
  for f in input_api.AffectedFiles():
    local_path = f.LocalPath()
    if local_path.endswith('cq.cfg'):
      touches_cq = True

  if touches_cq:
    commands.append(
      input_api.Command(
        name='cq.cfg presubmit', cmd=[
            input_api.python_executable, input_api.os_path.join(
                'branch', 'cq_cfg_presubmit.py'),
            '--check'],
        kwargs={}, message=output_api.PresubmitError),
    )

  commands.extend(input_api.canned_checks.GetUnitTestsRecursively(
      input_api, output_api,
      input_api.os_path.join(input_api.PresubmitLocalPath()),
      whitelist=[r'.+_unittest\.py$'], blacklist=[]))

  results = []

  results.extend(input_api.RunTests(commands))
  results.extend(input_api.canned_checks.CheckChangedLUCIConfigs(
      input_api, output_api))

  return results

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
