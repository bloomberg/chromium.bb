# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for isolate.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""


def CommonChecks(input_api, output_api):
  output = []
  disabled_warnings = [
    'R0401',  # Cyclic import
    'W0613',  # Unused argument
    'E1103',  # subprocess.communicate() generates these :(
  ]
  black_list = [r'src/site_scons/.*',
                r'.*/decode_dump.py',
                r'src/build_tools/build_sdk.py',
                r'src/build_tools/buildbot_common.py',
                r'src/build_tools/buildbot_run.py',
                r'src/build_tools/build_utils.py',
                r'src/build_tools/build_updater.py',
                r'src/build_tools/manifest_util.py',
                r'src/build_tools/nacl*',
                r'src/build_tools/generate_make.py',
                r'src/build_tools/sdk_tools/sdk_update_main.py',
                r'src/build_tools/sdk_tools/sdk_update.py',
                r'src/project_templates',
                r'.*/update_manifest.py',
                r'.*/update_nacl_manifest.py',
                r'src/build_tools/tests/.*']
  canned = input_api.canned_checks
  output.extend(canned.RunPylint(input_api, output_api, black_list=black_list,
                disabled_warnings=disabled_warnings))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
