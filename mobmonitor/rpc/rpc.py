# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module allows for communicating with the Mob* Monitor via RPC."""

from __future__ import print_function

import urllib
import urllib2

from chromite.lib import remote_access
from chromite.lib import retry_util


URLLIB_CALL_FORMAT_STR = '%(host)s/%(func)s/?%(args)s'
RPC_RETRY_TIMES = 5
RPC_SLEEP_SECS = 2
RPC_LIST = ['GetServiceList', 'GetStatus', 'RepairService']


class RpcError(Exception):
  """Raises when an error with preparing the RPC has been encountered."""


class RpcExecutor(object):
  """Construct and send RPCs to the Mob* Monitor with retry."""

  def __init__(self, host='localhost', port=9999):
    self.host = 'http://%s:%s' % (host, remote_access.NormalizePort(port))

  def ConstructUrllibCall(self, func, **kwargs):
    """Build a Mob* Monitor RPC to be used with urllib.

    Args:
      func: The remote function to call.
      kwargs: The arguments to the remote function func.

    Returns:
      A string used by urllib2 to use the Mob* Monitor's
      exposed RESTful interface.
    """
    # Create a string that can be used by urllib2 to interact
    # with the Mob* Monitor's RESTful interface.
    #
    # For example, suppose we have:
    #   host = 'http://localhost:9999'
    #   func = 'repair_service'
    #   kwargs = {'service': 's1', 'action': 'a1'}
    #
    # Then args becomes:
    #   'service=s1&action=a1'
    #
    # And we return:
    #   'http://localhost:9999/repair_service?service=s1&action=a1'
    #
    args = urllib.urlencode(kwargs)
    return URLLIB_CALL_FORMAT_STR % dict(host=self.host, func=func, args=args)

  def Execute(self, func, **kwargs):
    """Build and execute the RPC to the Mob* Monitor.

    Args:
      func: The remote function to call.
      kwargs: Arguments to above function.

    Returns:
      The result of the remote call.
    """
    def urllib_call():
      call = self.ConstructUrllibCall(func, **kwargs)
      return urllib2.urlopen(call).read()

    return retry_util.RetryException(urllib2.URLError, RPC_RETRY_TIMES,
                                     urllib_call, sleep=RPC_SLEEP_SECS)

  def GetServiceList(self):
    """List the monitored services.

    Returns:
      A list of the monitored services.
    """
    return self.Execute('GetServiceList')

  def GetStatus(self, service=None):
    """Get the service's health status.

    Args:
      service: A string. The service to query. If None, all services
        are queried.

    Returns:
      A namedtuple with the following fields:
        health_state: The service health state.
        description: A string which describes the health state.
        variables: A dictionary of variables pertaining to the service status.
        actions: A list of actions to take.

      If service is not None, a list of dictionaries is returned,
      one for each monitored service.
    """
    return self.Execute('GetStatus', service=service)

  # TODO (msartori): Implement crbug.com/505066.
  def RepairService(self, service=None, action=None):
    """Apply the specified action to the specified service.

    Args:
      service: A string. The service to repair.
      action: A string. The action to take.

    Returns:
      The same output of running get_status(service=service).
    """
    if service is None or action is None:
      raise RpcError('repair_service requires both the service'
                     ' and action to be provided.'
                     ' Given: service="%s" action="%s"' % (service, action))

    return self.Execute('RepairService', service=service, action=action)
