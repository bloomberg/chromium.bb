# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helpers for interacting with LUCI Logdog service."""

from __future__ import print_function

import base64
import json
import urllib

from chromite.lib.protos import annotations_pb2
from chromite.lib import prpc
from chromite.cbuildbot import topology


class LogdogResponseException(Exception):
  """Exception got from invalid Logdog response."""


class LogdogClient(prpc.PRPCClient):
  """Logdog client to interact with the LUCI Logdog service."""

  def _GetHost(self):
    """Get LUCI Logdog Server host from topology."""
    return topology.topology.get(topology.LUCI_LOGDOG_HOST_KEY)

  def LogsQueryAnnotations(self, master, builder, build_num, dryrun=False):
    """Logdog request to query for annotations pathname.

    Args:
      master: waterfall master to query.
      builder: builder to query.
      build_num: build number to query.
      dryrun: Whether a dryrun.

    Returns:
      annotations pathname or None if no streams are available.

    Raises:
      LogdogResponseException if response is invalid.
    """
    body = json.dumps({
        'project': master,
        'tags': {
            'buildbot.master': master,
            'buildbot.builder': builder,
            'buildbot.buildnumber': str(build_num),
        },
        'content_type': 'text/x-chrome-infra-annotations; version=2',
    })
    resp = self.SendRequest('prpc/logdog.Logs', 'Query', body, dryrun=dryrun)
    if 'streams' not in resp or len(resp['streams']) == 0:
      return None
    try:
      return resp['streams'][0]['path']
    except KeyError:
      raise LogdogResponseException('Unable to retrieve logs path')

  def LogsTail(self, project, path, dryrun=False):
    """Logdog Logs.Tail request to retrieve logs.

    Args:
      project: Logdog project of stream.
      path: Logdog path of stream.
      dryrun: Whether a dryrun.

    Returns:
      Full Logs.Tail response protobuf.
    """
    body = json.dumps({
        'project': project,
        'path': path,
    })
    return self.SendRequest('prpc/logdog.Logs', 'Tail', body, dryrun=dryrun)

  def GetAnnotations(self, master, builder, build_num, dryrun=False):
    """Retrieve Logdog annotations for a build.

    Args:
      master: waterfall master to query.
      builder: builder to query.
      build_num: build number to query.
      dryrun: Whether a dryrun.

    Returns:
      (Parsed Logdog annotations protobuf, Logdog prefix string) tuple
      or (None, None) if none is available.

    Raises:
      LogdogResponseException if response is invalid.
    """
    # Determine path to annotations.
    path = self.LogsQueryAnnotations(master, builder, build_num, dryrun=dryrun)
    if path is None:
      return None, None
    (prefix, _) = path.split('/+/')

    # Retrieve annotations datagram.
    annotations_logs = self.LogsTail(master, path)
    try:
      datagram = annotations_logs['logs'][0]['datagram']['data']
    except KeyError:
      raise LogdogResponseException('Unable to retrieve annotation datagram')

    # Parse annotations protobuf.
    annotations = annotations_pb2.Step()
    annotations.ParseFromString(base64.b64decode(datagram))
    return annotations, prefix

  def ConstructViewerURL(self, project, prefix, name):
    """Construct URL to view logs via Logdog.

    Args:
      project: Logdog project of log stream.
      prefix: Logdog prefix name.
      name: Logdog stream name.

    Returns:
      URL of logs viewer.
    """
    stream = '%s/+/%s' % (prefix, name)
    return '%(scheme)s://%(host)s/v/?s=%(query)s' % {
        'scheme': self.GetScheme(),
        'host': self.host,
        'query': urllib.quote_plus('%s/%s' % (project, stream)),
    }
