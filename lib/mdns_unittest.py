# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the mdns.py module."""

from __future__ import print_function

import dpkt
import mock
import select
import socket

from chromite.lib import cros_test_lib
from chromite.lib import mdns


class BadService(object):
  """A bad service used to signal the test to generate bad mDNS response."""


class mDNSTest(cros_test_lib.TestCase):
  """Tests for FindServices()"""
  def _BuildmDNSResponse(self, service):
    answers = []
    if service.ip:
      answers.append(
          dpkt.dns.DNS.RR(type=dpkt.dns.DNS_A, ip=socket.inet_aton(service.ip)))
    if service.ptrname:
      answers.append(
          dpkt.dns.DNS.RR(type=dpkt.dns.DNS_PTR, ptrname=service.ptrname))
    if service.hostname:
      answers.append(
          dpkt.dns.DNS.RR(type=dpkt.dns.DNS_SRV, srvname=service.hostname,
                          priority=1, weight=1, port=service.port))
    if service.text:
      text = ['%s=%s' % (key, value) for key, value in service.text.iteritems()]
      answers.append(dpkt.dns.DNS.RR(type=dpkt.dns.DNS_TXT, text=text))
    return dpkt.dns.DNS(op=dpkt.dns.DNS_QUERY,
                        rcode=dpkt.dns.DNS_RCODE_NOERR,
                        q=[], an=answers, ns=[])

  def _BuildBadmDNSResponse(self):
    return 'junk'

  def _TestmDNSResults(self, services, expected_results, should_add_func=None,
                       should_continue_func=None):
    """Mock out network responses and call FindServices().

    Args:
      services: List of services to return in mDNS responses.
      expected_results: List of services to expect from FindServices().
      should_add_func: See |should_add_func| argument in FindServices().
      should_continue_func: See |should_continue_func| argument in
        FindServices().
    """
    with mock.patch('socket.socket') as mock_socket_class:
      select.select = mock.MagicMock()
      mock_socket = mock_socket_class.return_value

      select_side_effects = []
      recvfrom_side_effects = []
      for service in services:
        select_side_effects.append(([1], [], []))
        if type(service) is mdns.Service:
          mdns_response = self._BuildmDNSResponse(service)
        else:
          mdns_response = self._BuildBadmDNSResponse()
        recvfrom_side_effects.append((str(mdns_response), ''))
      select_side_effects.append(([], [], []))
      select.select.side_effect = select_side_effects
      mock_socket.recvfrom.side_effect = recvfrom_side_effects

      results = mdns.FindServices('127.0.0.1',
                                  'a.local',
                                  should_add_func=should_add_func,
                                  should_continue_func=should_continue_func)
      self.assertEqual(results, expected_results)

  def testFindServices(self):
    """Test finding all mDNS services."""
    services = [mdns.Service('test1.local', '10.0.0.1', 1234, 'test1.a.local',
                             {'name': 'test'}),
                mdns.Service('test2.local', '10.0.0.2', 1234, 'test2.a.local',
                             {'name': 'test2'})]
    self._TestmDNSResults(services, services)

  def testFindServicesIncompleteResponse(self):
    """Test finding all mDNS services but ignoring incomplete services."""
    services = [mdns.Service('test1.local', '10.0.0.1', 1234, 'test1.a.local',
                             {'name': 'test'}),
                mdns.Service('test2.local', '10.0.0.2', 1234, None, None),
                mdns.Service('test3.local', '10.0.0.3', 1234, 'test3.a.local',
                             {'name': 'test3'})]
    expected_results = [services[0], services[2]]
    self._TestmDNSResults(services, expected_results)

  def testFindOneService(self):
    """Test finding a specific service."""
    services = [mdns.Service('test1.local', '10.0.0.1', 1234, 'test1.a.local',
                             {'name': 'test'}),
                mdns.Service('test2.local', '10.0.0.2', 1234, 'test2.a.local',
                             {'name': 'test2'})]
    expected_results = [services[1]]

    should_add_func = lambda x: x.hostname == services[1].hostname
    should_continue_func = lambda x: x.hostname != services[1].hostname
    self._TestmDNSResults(services, expected_results,
                          should_add_func=should_add_func,
                          should_continue_func=should_continue_func)

  def testFindSeveralServices(self):
    """Test early-exit condition.

    Accept all entries and make sure nothing is returned after matched entry.
    """
    services = [mdns.Service('test1.local', '10.0.0.1', 1234, 'test1.a.local',
                             {'name': 'test'}),
                mdns.Service('test2.local', '10.0.0.2', 1234, 'test2.a.local',
                             {'name': 'test2'}),
                mdns.Service('test3.local', '10.0.0.3', 1234, 'test3.a.local',
                             {'name': 'test3'}),
                mdns.Service('test4.local', '10.0.0.4', 1234, 'test4.a.local',
                             {'name': 'test4'})]
    expected_results = [services[0], services[1]]

    should_add_func = lambda x: True
    should_continue_func = lambda x: x.hostname != services[1].hostname
    self._TestmDNSResults(services, expected_results,
                          should_add_func=should_add_func,
                          should_continue_func=should_continue_func)

  def testBadResponse(self):
    """Test bad mDNS response."""
    services = [mdns.Service('test1.local', '10.0.0.1', 1234, 'test1.a.local',
                             {'name': 'test'}),
                BadService(),
                mdns.Service('test3.local', '10.0.0.3', 1234, 'test3.a.local',
                             {'name': 'test3'})]
    expected_results = [services[0], services[2]]
    self._TestmDNSResults(services, expected_results)
