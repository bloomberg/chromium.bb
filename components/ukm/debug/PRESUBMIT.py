# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CommonChecks(input_api, output_api):
  import web_dev_style.presubmit_support
  return (
      web_dev_style.presubmit_support.CheckStyleESLint(input_api, output_api) +
      input_api.canned_checks.CheckPatchFormatted(
          input_api, output_api, check_js=True))


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

