#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script was originally written by Alok Priyadarshi (alokp@)
# with some minor local modifications.

import contextlib
import json
import logging
import math
import optparse
import os
import sys
import websocket


class TracingClient(object):
  def BufferUsage(self, buffer_usage):
    percent = int(math.floor(buffer_usage * 100))
    logging.debug('Buffer Usage: %i', percent)


class TracingBackend(object):
  def __init__(self, devtools_port):
    self._socket = None
    self._next_request_id = 0
    self._tracing_client = None
    self._tracing_data = []

  def Connect(self, device_ip, devtools_port, timeout=10):
    assert not self._socket
    url = 'ws://%s:%i/devtools/browser' % (device_ip, devtools_port)
    print('Connect to %s ...' % url)
    self._socket = websocket.create_connection(url, timeout=timeout)
    self._next_request_id = 0

  def Disconnect(self):
    if self._socket:
      self._socket.close()
      self._socket = None

  def StartTracing(self,
                   tracing_client=None,
                   custom_categories=None,
                   record_continuously=False,
                   buffer_usage_reporting_interval=0,
                   timeout=10):
    self._tracing_client = tracing_client
    self._socket.settimeout(timeout)
    req = {
      'method': 'Tracing.start',
      'params': {
        'categories': custom_categories,
        'bufferUsageReportingInterval': buffer_usage_reporting_interval,
        'options': 'record-continuously' if record_continuously else
                   'record-until-full'
      }
    }
    self._SendRequest(req)

  def StopTracing(self, timeout=30):
    self._socket.settimeout(timeout)
    req = {'method': 'Tracing.end'}
    self._SendRequest(req)
    while self._socket:
      res = self._ReceiveResponse()
      if 'method' in res and self._HandleResponse(res):
        self._tracing_client = None
        result = self._tracing_data
        self._tracing_data = []
        return result

  def _SendRequest(self, req):
    req['id'] = self._next_request_id
    self._next_request_id += 1
    data = json.dumps(req)
    self._socket.send(data)

  def _ReceiveResponse(self):
    while self._socket:
      data = self._socket.recv()
      res = json.loads(data)
      return res

  def _HandleResponse(self, res):
    method = res.get('method')
    value = res.get('params', {}).get('value')
    if 'Tracing.dataCollected' == method:
      if type(value) in [str, unicode]:
        self._tracing_data.append(value)
      elif type(value) is list:
        self._tracing_data.extend(value)
      else:
        logging.warning('Unexpected type in tracing data')
    elif 'Tracing.bufferUsage' == method and self._tracing_client:
      self._tracing_client.BufferUsage(value)
    elif 'Tracing.tracingComplete' == method:
      return True


@contextlib.contextmanager
def Connect(device_ip, devtools_port):
  backend = TracingBackend(devtools_port)
  try:
    backend.Connect(device_ip, devtools_port)
    yield backend
  finally:
    backend.Disconnect()


def DumpTrace(trace, options):
  filepath = os.path.expanduser(options.output) if options.output \
      else os.path.join(os.getcwd(), 'trace.json')

  dirname = os.path.dirname(filepath)
  if dirname:
    if not os.path.exists(dirname):
      os.makedirs(dirname)
  else:
    filepath = os.path.join(os.getcwd(), filepath)

  with open(filepath, "w") as f:
   json.dump(trace, f)
  return filepath


def _CreateOptionParser():
  parser = optparse.OptionParser(description='Record about://tracing profiles '
                                 'from any running instance of Chrome.')
  parser.add_option(
      '-v', '--verbose', help='Verbose logging.', action='store_true')
  parser.add_option(
      '-p', '--port', help='Remote debugging port.', type="int", default=9222)
  parser.add_option(
      '-d', '--device', help='Device ip address.', type='string',
      default='127.0.0.1')

  tracing_opts = optparse.OptionGroup(parser, 'Tracing options')
  tracing_opts.add_option(
      '-c', '--category-filter',
      help='Apply filter to control what category groups should be traced.',
      type='string')
  tracing_opts.add_option(
      '--record-continuously',
      help='Keep recording until stopped. The trace buffer is of fixed size '
           'and used as a ring buffer. If this option is omitted then '
           'recording stops when the trace buffer is full.',
      action='store_true')
  parser.add_option_group(tracing_opts)

  output_options = optparse.OptionGroup(parser, 'Output options')
  output_options.add_option(
      '-o', '--output',
      help='Save trace output to file.')
  parser.add_option_group(output_options)

  return parser


def _ProcessOptions(options):
  websocket.enableTrace(options.verbose)


def main():
  parser = _CreateOptionParser()
  options, _args = parser.parse_args()
  _ProcessOptions(options)

  with Connect(options.device, options.port) as tracing_backend:
    tracing_backend.StartTracing(TracingClient(),
                                 options.category_filter,
                                 options.record_continuously)
    raw_input('Capturing trace. Press Enter to stop...')
    trace = tracing_backend.StopTracing()

  filepath = DumpTrace(trace, options)
  print('Done')
  print('Trace written to file://%s' % filepath)


if __name__ == '__main__':
  sys.exit(main())
