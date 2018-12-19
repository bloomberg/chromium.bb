# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces luci-milo.cfg consistency.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def _CheckLuciMiloCfg(input_api, output_api):
  if ('infra/config/global/luci-milo.cfg' not in input_api.LocalPaths() and
      'infra/config/global/lint-luci-milo.py' not in input_api.LocalPaths()):
    return []

  return input_api.RunTests([
      input_api.Command(
          name='lint-luci-milo',
          cmd=[input_api.python_executable, 'lint-luci-milo.py'],
          kwargs={},
          message=output_api.PresubmitError),
      # Technically doesn't rely on lint-luci-milo.py, but is a lightweight
      # enough check it should be fine to trigger.
      input_api.Command(
        name='testing/buildbot config checks',
        cmd=[input_api.python_executable, input_api.os_path.join(
                '..', '..', '..', 'testing', 'buildbot',
                'generate_buildbot_json.py',),
            '--check'],
        kwargs={}, message=output_api.PresubmitError)])

def CheckChangeOnUpload(input_api, output_api):
  return _CheckLuciMiloCfg(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CheckLuciMiloCfg(input_api, output_api)
