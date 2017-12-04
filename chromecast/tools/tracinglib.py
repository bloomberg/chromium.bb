# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Utilities for capturing traces for chromecast devices."""

import json
import logging
import math
import requests
import subprocess
import time
import websocket


class TracingClient(object):

  def BufferUsage(self, buffer_usage):
    percent = int(math.floor(buffer_usage * 100))
    logging.debug('Buffer Usage: %i', percent)


class TracingBackend(object):
  """Class for starting a tracing session with cast_shell."""

  def __init__(self, device_ip, devtools_port, timeout,
               buffer_usage_reporting_interval):
    """
    Args:
      device_ip: IP of device to connect to.
      devtools_port: Remote dev tool port to connect to. Defaults to 9222.
      timeout: Time to wait to start tracing in seconds. Default 10s.
      buffer_usage_reporting_interval: How often to report buffer usage.
    """
    self._socket = None
    self._next_request_id = 0
    self._tracing_client = None
    self._tracing_data = []
    self._device_ip = device_ip
    self._devtools_port = devtools_port
    self._timeout = timeout
    self._buffer_usage_reporting_interval = buffer_usage_reporting_interval
    self._included_categories = []
    self._excluded_categories = []

  def Connect(self):
    """Connect to cast_shell."""
    assert not self._socket
    # Get the secure browser debugging target.
    r = requests.get(
        'http://%s:%i/json/version' % (self._device_ip, self._devtools_port))
    url = r.json()['webSocketDebuggerUrl']
    print('Connect to %s ...' % url)
    self._socket = websocket.create_connection(url, timeout=self._timeout)
    self._next_request_id = 0

  def Disconnect(self):
    """If connected to device, disconnect from device."""
    if self._socket:
      self._socket.close()
      self._socket = None

  def StartTracing(self,
                   custom_categories=None,
                   record_continuously=False):
    """Begin a tracing session on device.

    Args:
      custom_categories: Categories to filter for. None records all categories.
      record_continuously: Keep tracing until stopped. If false, will exit when
                           buffer is full.
    """
    self._tracing_client = TracingClient()
    self._socket.settimeout(self._timeout)
    self._ParseCustomCategories(custom_categories)
    req = {
        'method': 'Tracing.start',
        'params': {
            'transferMode': 'ReportEvents',
            'traceConfig': {
                'recordMode':
                    'recordContinuously'
                    if record_continuously else 'recordUntilFull',
                'includedCategories':
                    self._included_categories,
                'excludedCategories':
                    self._excluded_categories,
            },
            'bufferUsageReportingInterval':
                self._buffer_usage_reporting_interval,
        }
    }
    self._SendRequest(req)

  def StopTracing(self, output_file):
    """End a tracing session on device.

    Args:
      output_file: Path to the file to store the trace.
    """
    self._socket.settimeout(self._timeout)
    req = {'method': 'Tracing.end'}
    self._SendRequest(req)
    while self._socket:
      res = self._ReceiveResponse()
      has_error = 'error' in res
      if has_error:
        logging.error('Tracing error: ' + str(res.get('error')))
      if has_error or ('method' in res and self._HandleResponse(res)):
        self._tracing_client = None
        result = self._tracing_data
        self._tracing_data = []

        with open(output_file, 'w') as f:
          json.dump(result, f)
        return

  def _SendRequest(self, req):
    """Sends request to remote devtools.

    Args:
      req: Request to send.
    """
    req['id'] = self._next_request_id
    self._next_request_id += 1
    data = json.dumps(req)
    self._socket.send(data)

  def _ReceiveResponse(self):
    """Get response from remote devtools.

    Returns:
      Response received.
    """
    while self._socket:
      data = self._socket.recv()
      res = json.loads(data)
      return res

  def _HandleResponse(self, res):
    """Handle response from remote devtools.

    Args:
      res: Recieved tresponse that should be handled.
    """
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

  def _ParseCustomCategories(self, custom_categories):
    """Parse a category filter into trace config format"""

    self._included_categories = []
    self._excluded_categories = []

    # See TraceConfigCategoryFilter::InitializeFromString in chromium.
    categories = (token.strip() for token in custom_categories.split(','))
    for category in categories:
      if not category:
        continue
      if category.startswith('-'):
        self._excluded_categories.append(category[1:])
      else:
        self._included_categories.append(category)


class TracingBackendAndroid(object):
  """Android version of TracingBackend."""
  def __init__(self, device):
    self.device = device

  def Connect(self):
    pass


  def Disconnect(self):
    pass

  def StartTracing(self,
                   custom_categories=None,
                   record_continuously=False):
    """Begin a tracing session on device.

    Args:
      custom_categories: Categories to filter for. None records all categories.
      record_continuously: Keep tracing until stopped. If false, will exit when
                           buffer is full.
    """
    categories = (custom_categories if custom_categories else
                  '_DEFAULT_CHROME_CATEGORIES')
    self._file = '/sdcard/Download/trace-py-{0}'.format(int(time.time()))
    command = ['shell', 'am', 'broadcast',
        '-a', 'com.google.android.apps.mediashell.GPU_PROFILER_START',
        '-e', 'categories', categories,
        '-e', 'file', self._file]
    if record_continuously:
      command += ['-e', 'continuous']

    self._AdbCommand(command)

  def StopTracing(self, output_file):
    """End a tracing session on device.

    Args:
      output_file: Path to the file to store the trace.
    """
    stop_profiling_command = ['shell', 'am', 'broadcast',
        '-a', 'com.google.android.apps.mediashell.GPU_PROFILER_STOP']
    self._AdbCommand(stop_profiling_command)

    # Wait for trace file to be written
    while True:
      result = self._AdbCommand(['logcat', '-d'])
      if 'Results are in %s' % self._file in result:
        break

    self._AdbCommand(['pull', self._file, output_file])

  def _AdbCommand(self, command):
    args = ['adb', '-s', self.device]
    logging.debug(' '.join(args + command))
    result = subprocess.check_output(args + command)
    logging.debug(result)
    return result
