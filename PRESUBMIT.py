#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

UNIT_TESTS = [
  'tests.gcl_unittest',
  'tests.gclient_test',
  'tests.gclient_scm_test',
  'tests.gclient_utils_test',
  'tests.presubmit_unittest',
  'tests.revert_unittest',
  'tests.scm_unittest',
  'tests.trychange_unittest',
  'tests.watchlists_unittest',
]

def CheckChangeOnUpload(input_api, output_api):
  output = []
  output.extend(input_api.canned_checks.RunPythonUnitTests(input_api,
                                                           output_api,
                                                           UNIT_TESTS))
  return output


def CheckChangeOnCommit(input_api, output_api):
  output = []
  output.extend(input_api.canned_checks.RunPythonUnitTests(input_api,
                                                           output_api,
                                                           UNIT_TESTS))
  output.extend(input_api.canned_checks.CheckDoNotSubmit(input_api,
                                                         output_api))
  return output
