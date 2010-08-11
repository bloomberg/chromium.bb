# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

UNIT_TESTS = [
  'tests.gcl_unittest',
  'tests.gclient_scm_test',
  'tests.gclient_smoketest',
  'tests.gclient_utils_test',
  'tests.presubmit_unittest',
  'tests.scm_unittest',
  'tests.trychange_unittest',
  'tests.watchlists_unittest',
]

def CheckChangeOnUpload(input_api, output_api):
  output = []
  output.extend(input_api.canned_checks.RunPythonUnitTests(input_api,
                                                           output_api,
                                                           UNIT_TESTS))
  output.extend(WasGitClUploadHookModified(input_api, output_api))
  return output


def CheckChangeOnCommit(input_api, output_api):
  output = []
  output.extend(input_api.canned_checks.RunPythonUnitTests(input_api,
                                                           output_api,
                                                           UNIT_TESTS))
  output.extend(input_api.canned_checks.CheckDoNotSubmit(input_api,
                                                         output_api))
  output.extend(WasGitClUploadHookModified(input_api, output_api))
  return output

def WasGitClUploadHookModified(input_api, output_api):
  for affected_file in input_api.AffectedSourceFiles(None):
    if (input_api.os_path.basename(affected_file.LocalPath()) ==
        'git-cl-upload-hook'):
      return [output_api.PresubmitPromptWarning(
          'Don\'t forget to fix git-cl to download the newest version of '
          'git-cl-upload-hook')]
  return []
