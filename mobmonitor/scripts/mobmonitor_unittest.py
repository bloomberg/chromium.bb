# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the main Mob* Monitor script."""

from __future__ import print_function

import json

from chromite.lib import cros_test_lib
from chromite.mobmonitor.checkfile import manager
from chromite.mobmonitor.scripts import mobmonitor


class MockCheckFileManager(object):
  """Mock CheckFileManager object that returns 'real' responses for testing."""

  def __init__(self):
    failed_check = manager.HEALTHCHECK_STATUS('hc1', False, 'Failed', [])

    self.service_statuses = [
        manager.SERVICE_STATUS('service1', True, []),
        manager.SERVICE_STATUS('service2', False, [failed_check])]

  def GetServiceList(self):
    """Mock GetServiceList response."""
    return ['test_service_1', 'test_service_2']

  def GetStatus(self, service=None):
    """Mock GetStatus response."""
    if service is None:
      return self.service_statuses

    return self.service_statuses[0]

  def RepairService(self, _service, _action, _args, _kwargs):
    """Mock RepairService response."""
    return self.service_statuses[0]


class MobMonitorRootTest(cros_test_lib.MockTestCase):
  """Unittests for the MobMonitorRoot."""

  def testGetServiceList(self):
    """Test the GetServiceList RPC."""
    cfm = MockCheckFileManager()
    root = mobmonitor.MobMonitorRoot(cfm)
    self.assertEqual(cfm.GetServiceList(), json.loads(root.GetServiceList()))

  def testGetStatus(self):
    """Test the GetStatus RPC."""
    cfm = MockCheckFileManager()
    root = mobmonitor.MobMonitorRoot(cfm)

    # Test the result for a single service.
    status = cfm.service_statuses[0]
    expect = {'service': status.service, 'health': status.health,
              'healthchecks': []}
    self.assertEquals([expect], json.loads(root.GetStatus(status.service)))

    # Test the result for multiple services.
    status1, status2 = cfm.service_statuses
    check = status2.healthchecks[0]
    expect = [{'service': status1.service, 'health': status1.health,
               'healthchecks': []},
              {'service': status2.service, 'health': status2.health,
               'healthchecks': [{'name': check.name, 'health': check.health,
                                 'description': check.description,
                                 'actions': []}]}]
    self.assertEquals(expect, json.loads(root.GetStatus()))

  def testRepairService(self):
    """Test the RepairService RPC."""
    cfm = MockCheckFileManager()
    root = mobmonitor.MobMonitorRoot(cfm)

    status = cfm.service_statuses[0]
    expect = {'service': status.service, 'health': status.health,
              'healthchecks': []}
    string_args = '[1, 2]'
    string_kwargs = '{"a": 1}'
    self.assertEquals(expect,
                      json.loads(root.RepairService('dummy_service',
                                                    'dummy_action',
                                                    string_args,
                                                    string_kwargs)))
