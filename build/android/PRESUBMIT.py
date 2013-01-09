# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for android buildbot.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

def CommonChecks(input_api, output_api):
  output = []

  def J(*dirs):
    """Returns a path relative to presubmit directory."""
    return input_api.os_path.join(input_api.PresubmitLocalPath(), *dirs)

  output.extend(input_api.canned_checks.RunPylint(
      input_api,
      output_api,
      white_list=[r'PRESUBMIT\.py$', r'buildbot/.*\.py$'],
      extra_paths_list=[J()]))

  output.extend(input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, J('buildbot', 'tests')))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
