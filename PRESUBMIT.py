# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Top-level presubmit script for GYP.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""


def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api))
  report.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api, output_api,
      'http://gyp-status.appspot.com/status',
      'http://gyp-status.appspot.com/current'))
  return report


def GetPreferredTrySlaves():
  return ['gyp-win32', 'gyp-win64', 'gyp-linux', 'gyp-mac']
