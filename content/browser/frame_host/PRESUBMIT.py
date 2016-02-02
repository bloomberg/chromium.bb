# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //content/browser/frame_host.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import re


def _GetTryMasters(project, change):
  return {
    'tryserver.chromium.linux': {
      'linux_site_isolation': [],
     },
  }


def GetPreferredTryMasters(project, change):
  # TODO(nick, dcheng): We don't a value here because it would replace, rather
  # than augment, the default set of try servers. Figure out how to augment it.
  return {}


def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds extra try bots to the CL description in order to run site
  isolation tests in addition to CQ try bots.
  """
  rietveld_obj = cl.RpcServer()
  issue = cl.issue
  description = rietveld_obj.get_description(issue)
  if re.search(r'^CQ_INCLUDE_TRYBOTS=.*', description, re.M | re.I):
    return []

  masters = _GetTryMasters(None, change)
  results = []
  new_description = description
  new_description += '\nCQ_INCLUDE_TRYBOTS=%s' % ';'.join(
      '%s:%s' % (master, ','.join(bots))
      for master, bots in masters.iteritems())
  results.append(output_api.PresubmitNotifyResult(
      'Automatically added site isolation trybots to run tests on CQ.'))

  rietveld_obj.update_description(issue, new_description)

  return results
