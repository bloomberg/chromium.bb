# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Assistant.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.
  """

  results = []
  issue = cl.issue
  if issue:
    description_lines, footers = cl.GetDescriptionFooters()
    trybot_tag = 'CQ_INCLUDE_TRYBOTS=luci.chrome.try:linux-chromeos-chrome'
    if trybot_tag not in description_lines:
      description_lines.append('')
      description_lines.append(trybot_tag)
      cl.UpdateDescriptionFooters(description_lines, footers)

  return results

