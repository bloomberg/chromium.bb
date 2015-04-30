# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that enumerates mDNS services."""

from __future__ import print_function

import collections
import dpkt
import select
import socket
import time


MDNS_MULTICAST_ADDRESS = '224.0.0.251'
MDNS_PORT = 5353
PACKET_BUFFER_SIZE = 2048


Service = collections.namedtuple(
    'Service',
    ('hostname', 'ip', 'port', 'ptrname', 'text'))


def _ParseResponse(data):
  """Parse mDNS network response into Service tuple.

  Args:
    data: Byte-blob of mDNS response.

  Returns:
    None if the mDNS response is invalid or incomplete, else a fully populated
    |Service| tuple.
  """
  hostname = None
  ip = None
  port = None
  ptrname = None
  text = None

  try:
    dns_response = dpkt.dns.DNS(data)
  except (dpkt.Error, dpkt.NeedData, dpkt.UnpackError):
    # Ignore bad mDNS response.
    return None

  for rr in dns_response.an:
    if rr.type == dpkt.dns.DNS_A:
      ip = socket.inet_ntoa(rr.ip)
    elif rr.type == dpkt.dns.DNS_PTR:
      ptrname = rr.ptrname
    elif rr.type == dpkt.dns.DNS_SRV:
      hostname = rr.srvname
      port = rr.port
    elif rr.type == dpkt.dns.DNS_TXT:
      text = dict(entry.split('=', 1) for entry in rr.text)
  service = Service(hostname, ip, port, ptrname, text)

  # Ignore incomplete responses.
  if any(x is None for x in service):
    return None
  return service


def FindServices(source_ip, service_name, should_add_func=None,
                 should_continue_func=None, timeout_seconds=1):
  """Find all instances of |service_name| on the network.

  For each service found, |should_add_func| is called with the service
  information to determine wheter the service should be added to the results
  list.  This method exits after |timeout_seconds| or earlier if instructed by
  the return value from |should_continue_func|.

  Args:
    source_ip: IP address of the network interface to use for service discovery.
    service_name: Name of mDNS service to discover (eg. '_ssh._tcp.local').
    should_add_func: Function called for each service found to determine if the
      service should be added to the results list.  If None is specified, all
      services found are added to the results list.
    should_continue_func: Function called for each service found to determine
      whether to continue with service discovery.  If None is specified,
      service discovery continues until the timeout expires.
    timeout_seconds: Number of seconds to wait for services to respond to
      discovery request.

  Returns:
    List of |Service| found.
  """
  # Default callback functions that add all services to the results list and
  # continue service discovery until the timeout expires.
  if not should_add_func:
    should_add_func = lambda service: True
  if not should_continue_func:
    should_continue_func = lambda service: True

  net_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  net_socket.bind((source_ip, 0))
  query = [dpkt.dns.DNS.Q(name=service_name, type=dpkt.dns.DNS_PTR)]
  dns_packet = dpkt.dns.DNS(op=dpkt.dns.DNS_QUERY, qd=query)
  net_socket.sendto(str(dns_packet),
                    (MDNS_MULTICAST_ADDRESS, MDNS_PORT))

  results = []
  remaining_time = timeout_seconds
  end_time = time.time() + remaining_time
  while remaining_time > 0:
    read_ready, _, _ = select.select([net_socket], [], [], remaining_time)
    if not read_ready:
      break

    data, _ = net_socket.recvfrom(PACKET_BUFFER_SIZE)
    service = _ParseResponse(data)
    if service:
      if should_add_func(service):
        results.append(service)
      if not should_continue_func(service):
        break

    remaining_time = end_time - time.time()

  return results
