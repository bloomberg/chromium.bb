# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'Presubmit script for changes affecting chrome/browser/resources/local_ntp/'

def NTPPresubmitChecks(input_api, output_api):
  affected = input_api.change.AffectedFiles()
  warning_str = ('Test HTML files in chrome/test/data/local_ntp/ should be '
                 'updated when making changes to local_ntp.html');
  if (any(f for f in affected if
          f.LocalPath().endswith('local_ntp.html')) and
      not any(f for f in affected if
              f.LocalPath().endswith('local_ntp_browsertest.html'))):
    return [output_api.PresubmitPromptWarning(warning_str)]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return NTPPresubmitChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return NTPPresubmitChecks(input_api, output_api)
