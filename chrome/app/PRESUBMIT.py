# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Notify people of GRD changes."""

GRD_FILE_ENDING = 'resources.grd'

def CheckChangeOnCommit(input_api, output_api):
  results = []
  for f in input_api.AffectedFiles():
    if f.LocalPath().endswith(GRD_FILE_ENDING):
      results = [ output_api.PresubmitPromptWarning('Your CL includes a GRD '
          'change. Please notify the sheriff BEFORE landing.\nSee this page '
          'for more information:\n  '
          'http://dev.chromium.org/developers/tree-sheriffs/sheriff-details-'
          'chromium') ]
      break
  return results
