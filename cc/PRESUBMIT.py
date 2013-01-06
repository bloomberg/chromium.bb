# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for cc.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

import re

CC_SOURCE_FILES=(r'^cc/.*\.(cc|h)$',)

def CheckAsserts(input_api, output_api, white_list=CC_SOURCE_FILES, black_list=None):
  black_list = tuple(black_list or input_api.DEFAULT_BLACK_LIST)
  source_file_filter = lambda x: input_api.FilterSourceFile(x, white_list, black_list)

  assert_files = []
  notreached_files = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    # WebKit ASSERT() is not allowed.
    if re.search(r"\bASSERT\(", contents):
      assert_files.append(f.LocalPath())
    # WebKit ASSERT_NOT_REACHED() is not allowed.
    if re.search(r"ASSERT_NOT_REACHED\(", contents):
      notreached_files.append(f.LocalPath())

  if assert_files:
    return [output_api.PresubmitError(
      'These files use ASSERT instead of using DCHECK:',
      items=assert_files)]
  if notreached_files:
    return [output_api.PresubmitError(
      'These files use ASSERT_NOT_REACHED instead of using NOTREACHED:',
      items=notreached_files)]
  return []

def CheckSpamLogging(input_api, output_api, white_list=CC_SOURCE_FILES, black_list=None):
  black_list = tuple(black_list or input_api.DEFAULT_BLACK_LIST)
  source_file_filter = lambda x: input_api.FilterSourceFile(x, white_list, black_list)

  bad_files = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if re.search(r"\bD?LOG\s*\(\s*INFO\s*\)", contents):
      bad_files.append(f.LocalPath())

  if bad_files:
    return [output_api.PresubmitError(
      'These files spam the console log with LOG(INFO):',
      items=bad_files)]
  return []


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += CheckAsserts(input_api, output_api)
  results += CheckSpamLogging(input_api, output_api)
  return results

def GetPreferredTrySlaves(project, change):
  return [
    'linux_layout_rel',
    ]
