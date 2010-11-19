# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def CheckChange(input_api, output_api, committing):
  # We need to change the path so that we can import ceee_presubmit
  # which lies at the root of the ceee folder. And we do it here so that
  # it doesn't pollute all the cases where we get imported yet not called.
  sys.path.append(os.path.join(input_api.PresubmitLocalPath(), '../ceee'))
  import ceee_presubmit
  return ceee_presubmit.CheckChange(input_api,
                                    output_api,
                                    committing,
                                    is_chrome_frame=True)


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api, False)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api, True)
