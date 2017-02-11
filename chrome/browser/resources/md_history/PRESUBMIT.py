# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CheckChangeOnUpload(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
      input_api, output_api, check_js=True)
