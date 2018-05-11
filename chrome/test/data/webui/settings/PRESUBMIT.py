# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CheckChangeOnUploadAndCommit(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                     check_js=True)


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadAndCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadAndCommit(input_api, output_api)
