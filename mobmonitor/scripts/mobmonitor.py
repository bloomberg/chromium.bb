#!/usr/bin/python2
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Mob* Monitor web interface."""

from __future__ import print_function

import cherrypy
import json
import sys

from chromite.lib import remote_access
from chromite.lib import commandline
from chromite.mobmonitor.checkfile import manager


class MobMonitorRoot(object):
  """The central object supporting the Mob* Monitor web interface."""

  def __init__(self, checkfile_manager):
    self.checkfile_manager = checkfile_manager

  @cherrypy.expose
  def index(self):
    """Presents a welcome message."""
    return 'Welcome to the Mob* Monitor!'

  @cherrypy.expose
  def GetServiceList(self):
    """Return a list of the monitored services.

    Returns:
      A list of the monitored services.
    """
    return json.dumps(self.checkfile_manager.GetServiceList())

  @cherrypy.expose
  def GetStatus(self, service=None):
    """Return the health status of the specified service.

    Args:
      service: The service whose health status is being queried. If service
        is None, return the health status of all monitored services.

    Returns:
      A list of dictionaries. Each dictionary contains the keys:
        service: The name of the service.
        health: A boolean describing the overall service health.
        healthchecks: A list of unhealthy or quasi-healthy health checks.
    """
    service_statuses = self.checkfile_manager.GetStatus(service)
    if not isinstance(service_statuses, list):
      service_statuses = [service_statuses]

    result = [
        manager.MapServiceStatusToDict(status) for status in service_statuses]
    return json.dumps(result)

  # TODO (msartori): Implement crbug.com/505066.
  @cherrypy.expose
  def RepairService(self, service, action):
    """Execute the repair action on the specified service.

    Args:
      service: The service that the specified action will be applied to.
      action: The action to be applied.
    """
    status = self.checkfile_manager.RepairService(service, action)
    return json.dumps(manager.MapServiceStatusToDict(status))


def ParseArguments(argv):
  """Creates the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('-d', '--checkdir',
                      default='/etc/mobmonitor/checkfiles/',
                      help='The Mob* Monitor checkfile directory.')
  parser.add_argument('-p', '--port', type=int, default=9999,
                      help='The Mob* Monitor port.')

  return parser.parse_args(argv)


def main(argv):
  options = ParseArguments(argv)
  options.Freeze()

  # Start the Mob* Monitor web interface.
  cherrypy.config.update({'server.socket_port':
                          remote_access.NormalizePort(options.port)})

  # Setup the mobmonitor
  checkfile_manager = manager.CheckFileManager(checkdir=options.checkdir)
  mobmonitor = MobMonitorRoot(checkfile_manager)

  # Start the checkfile collection and execution background task.
  checkfile_manager.StartCollectionExecution()

  cherrypy.quickstart(mobmonitor)


if __name__ == '__main__':
  main(sys.argv[1:])
