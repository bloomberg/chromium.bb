# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Documentation on PRESUBMIT.py can be found at:
# http://www.chromium.org/developers/how-tos/depottools/presubmit-scripts


def CheckChangeOnUpload(input_api, output_api):
  report = []
  affected_files = input_api.AffectedFiles(include_deletes=False)
  report.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api, project_name='Native Client'))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(CheckChangeOnUpload(input_api, output_api))
  report.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api, output_api,
      json_url='http://naclsdk-status.appspot.com/current?format=json'))
  return report
