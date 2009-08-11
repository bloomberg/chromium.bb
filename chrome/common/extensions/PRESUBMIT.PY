#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path

DEPENDENT_DIRS = set([
  os.path.normpath("chrome/common/extensions/api"),
  os.path.normpath("chrome/common/extensions/docs/build"),
  os.path.normpath("chrome/common/extensions/docs/js"),
  os.path.normpath("chrome/common/extensions/docs/static"),
  os.path.normpath("chrome/common/extensions/docs/template")
])

REBUILD_WARNING = """
This change modifies file(s) which the extension docs depend on. You must
rebuild the extension docs.

Build by running the build.py script in chrome/common/extensions/docs/build/.

Be sure to include any modified resulting static files
(/common/extension/docs/*.html) in your final changelist. 
"""

def CheckChange(input_api, output_api):  
  for f in input_api.AffectedFiles():
    dir = os.path.normpath(input_api.os_path.dirname(f.LocalPath()))
    if dir in DEPENDENT_DIRS:
      return [output_api.PresubmitPromptWarning(REBUILD_WARNING)]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)