#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib


def DiffFileContents(expected_path, actual_path, description):
  """Check file contents for equality and return the diff or None."""
  with open(expected_path) as f_expected, open(actual_path) as f_actual:
    expected_lines = f_expected.readlines()
    actual_lines = f_actual.readlines()

  if expected_lines == actual_lines:
    return None

  diff = difflib.unified_diff(
      expected_lines,
      actual_lines,
      fromfile=expected_path,
      tofile=actual_path,
      n=0)

  return """
Detected change in {}.
If change is expected, please update the expectations by running:

    cd out/Release
    cp {} {}

If you have hit this error on a bot and the error is for a public target,
build locally with enable_chrome_android_internal=false.

Here is the diff:
{}
""".format(description, actual_path, expected_path, '\n'.join(diff))
