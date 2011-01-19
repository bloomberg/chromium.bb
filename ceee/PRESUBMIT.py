# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys


def CheckChange(input_api, output_api, committing):
  # We need to change the path so that we can import ceee_presubmit
  # which lies at the root of the ceee folder. And we do it here so that
  # it doesn't pollute all the cases where we get imported yet not called.
  old_sys_path = sys.path
  ceee_path = input_api.os_path.join(input_api.PresubmitLocalPath(), '../ceee')
  try:
    sys.path = [ceee_path] + sys.path
    import ceee_presubmit
    return ceee_presubmit.CheckChange(input_api, output_api, committing)
  finally:
    sys.path = old_sys_path


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api, False)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api, True)
