# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for //chrome/browser/ui/views

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds extra try bots to the CL description in order to ensure
  the MacViews browser builds if Mac-specific files were touched.
  """
  def IsMacFile(f):
    file_name = os.path.split(f.LocalPath())[1]
    return file_name.find('_mac') != -1
  if not change.AffectedFiles(file_filter=IsMacFile):
    return []
  return output_api.EnsureCQIncludeTrybotsAreAdded(
    cl,
    [
      'master.tryserver.chromium.mac:mac_chromium_10.10_macviews'
    ],
    'Automatically added MacViews trybot to run tests on CQ.')
