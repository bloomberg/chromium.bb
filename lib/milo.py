# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helpers for interacting with LUCI Milo service."""

from __future__ import print_function

import base64
import json

from chromite.lib import prpc
from chromite.cbuildbot import topology


class MiloClient(prpc.PRPCClient):
  """Milo client to interact with the LUCI Milo service."""

  def _GetHost(self):
    """Get LUCI Milo Server host from topology."""
    return topology.topology.get(topology.LUCI_MILO_HOST_KEY)

  def GetBuildbotBuildJSON(self, master, builder, build_num, dryrun=False):
    """Get Buildbot build summary JSON file.

    Args:
      master: waterfall master to query.
      builder: builder to query.
      build_num: build number to query.
      dryrun: Whether a dryrun.

    Returns:
      Dictionary corresponding to parsed JSON file.
    """
    body = json.dumps({
        'master': master,
        'builder': builder,
        'build_num': int(build_num),
    })
    resp = self.SendRequest('prpc/milo.Buildbot', 'GetBuildbotBuildJSON',
                            body, dryrun=dryrun)
    data = base64.b64decode(resp['data'])
    if not data:
      return None
    result = json.loads(data)
    properties = {p[0] : p[1] for p in result['properties']}
    result['properties'] = properties
    steps = {step['name'] : step for step in result['steps']}
    result['steps'] = steps
    result['masterName'] = master
    return result
