# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Entry point to stats reporting objects for cbuildbot.

These factories setup the stats collection modules (es_utils, statsd) correctly
so that cbuildbot stats from different sources (official builders, trybots,
developer machines etc.) stay separate.
"""

from __future__ import print_function

from chromite.cbuildbot import constants
from chromite.cbuildbot import topology
from chromite.lib import factory
from chromite.lib.graphite_lib import es_utils
from chromite.lib.graphite_lib import stats
from chromite.lib.graphite_lib import stats_es_mock


CONNECTION_TYPE_DEBUG = 'debug'
CONNECTION_TYPE_MOCK = 'none'
CONNECTION_TYPE_PROD = 'prod'
CONNECTION_TYPE_READONLY = 'readonly'

# The types definitions below make linter unhappy. The 'right' way of using
# functools.partial makes functools.wraps (and hence our decorators) blow up.
# pylint: disable=unnecessary-lambda

class ESMetadataFactoryClass(factory.ObjectFactory):
  """Factory class for setting up an Elastic Search connection."""

  _ELASTIC_SEARCH_TYPES = {
      CONNECTION_TYPE_PROD: factory.CachedFunctionCall(
          lambda: es_utils.ESMetadata(
              use_http=constants.ELASTIC_SEARCH_USE_HTTP,
              host=topology.topology.get(topology.ELASTIC_SEARCH_HOST_KEY),
              port=topology.topology.get(topology.ELASTIC_SEARCH_PORT_KEY),
              index=constants.ELASTIC_SEARCH_INDEX,
              udp_port=topology.topology.get(
                  topology.ELASTIC_SEARCH_UDP_PORT_KEY))),
      CONNECTION_TYPE_READONLY: factory.CachedFunctionCall(
          lambda: es_utils.ESMetadataRO(
              use_http=constants.ELASTIC_SEARCH_USE_HTTP,
              host=topology.topology.get(topology.ELASTIC_SEARCH_HOST_KEY),
              port=topology.topology.get(topology.ELASTIC_SEARCH_PORT_KEY),
              index=constants.ELASTIC_SEARCH_INDEX,
              udp_port=topology.topology.get(
                  topology.ELASTIC_SEARCH_UDP_PORT_KEY)))
      }

  def __init__(self):
    super(ESMetadataFactoryClass, self).__init__(
        'elastic search connection', self._ELASTIC_SEARCH_TYPES,
        lambda from_setup, to_setup: from_setup == to_setup)

  def SetupProd(self):
    """Set up this factory to connect to the production Elastic Search."""
    self.Setup(CONNECTION_TYPE_PROD)

  def SetupReadOnly(self):
    """Set up this factory to allow querying the production Elastic Search."""
    self.Setup(CONNECTION_TYPE_READONLY)


ESMetadataFactory = ESMetadataFactoryClass()


class StatsFactoryClass(factory.ObjectFactory):
  """Factory class for setting up a Statsd connection."""

  _STATSD_TYPES = {
      CONNECTION_TYPE_PROD: factory.CachedFunctionCall(
          lambda: stats.Statsd(
              es=ESMetadataFactory.GetInstance(),
              host=topology.topology.get(topology.STATSD_HOST_KEY),
              port=topology.topology.get(topology.STATSD_PORT_KEY),
              prefix=constants.STATSD_PROD_PREFIX)),
      CONNECTION_TYPE_DEBUG: factory.CachedFunctionCall(
          lambda: stats.Statsd(
              es=ESMetadataFactory.GetInstance(),
              host=topology.topology.get(topology.STATSD_HOST_KEY),
              port=topology.topology.get(topology.STATSD_PORT_KEY),
              prefix=constants.STATSD_DEBUG_PREFIX)),
      CONNECTION_TYPE_MOCK: factory.CachedFunctionCall(
          lambda: stats_es_mock.Stats())
      }

  def __init__(self):
    super(StatsFactoryClass, self).__init__(
        'statsd connection', self._STATSD_TYPES,
        lambda from_setup, to_setup: from_setup == to_setup)

  def SetupProd(self):
    """Set up this factory to connect to the production Statsd."""
    self.Setup(CONNECTION_TYPE_PROD)

  def SetupDebug(self):
    """Set up this factory to connect to the debug Statsd."""
    self.Setup(CONNECTION_TYPE_DEBUG)

  def SetupMock(self):
    """Set up this factory to return a mock statsd object."""
    self.Setup(CONNECTION_TYPE_MOCK)


StatsFactory = StatsFactoryClass()
