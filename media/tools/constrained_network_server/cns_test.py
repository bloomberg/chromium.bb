#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for Constrained Network Server."""
import os
import signal
import subprocess
import tempfile
import time
import unittest
import urllib2

import cns


class PortAllocatorTest(unittest.TestCase):
  """Unit tests for the Port Allocator class."""

  # Expiration time for ports. In mock time.
  _EXPIRY_TIME = 6

  def setUp(self):
    # Mock out time.time() to accelerate port expiration testing.
    self._old_time = time.time
    self._current_time = 0
    time.time = lambda: self._current_time

    # TODO(dalecurtis): Mock out actual calls to shadi's port setup.
    self._pa = cns.PortAllocator(cns._DEFAULT_CNS_PORT_RANGE, self._EXPIRY_TIME)

  def tearDown(self):
    self._pa.Cleanup(all_ports=True)
    # Ensure ports are cleaned properly.
    self.assertEquals(self._pa._ports, {})
    time.time = self._old_time

  def testPortAllocator(self):
    # Ensure Get() succeeds and returns the correct port.
    self.assertEquals(self._pa.Get('test'), cns._DEFAULT_CNS_PORT_RANGE[0])

    # Call again with the same key and make sure we get the same port.
    self.assertEquals(self._pa.Get('test'), cns._DEFAULT_CNS_PORT_RANGE[0])

    # Call with a different key and make sure we get a different port.
    self.assertEquals(self._pa.Get('test2'), cns._DEFAULT_CNS_PORT_RANGE[0] + 1)

    # Update fake time so that ports should expire.
    self._current_time += self._EXPIRY_TIME + 1

    # Test to make sure cache is checked before expiring ports.
    self.assertEquals(self._pa.Get('test2'), cns._DEFAULT_CNS_PORT_RANGE[0] + 1)

    # Update fake time so that ports should expire.
    self._current_time += self._EXPIRY_TIME + 1

    # Request a new port, old ports should be expired, so we should get the
    # first port in the range. Make sure this is the only allocated port.
    self.assertEquals(self._pa.Get('test3'), cns._DEFAULT_CNS_PORT_RANGE[0])
    self.assertEquals(self._pa._ports.keys(), [cns._DEFAULT_CNS_PORT_RANGE[0]])

  def testPortAllocatorExpiresOnlyCorrectPorts(self):
    # Ensure Get() succeeds and returns the correct port.
    self.assertEquals(self._pa.Get('test'), cns._DEFAULT_CNS_PORT_RANGE[0])

    # Stagger port allocation and so we can ensure only ports older than the
    # expiry time are actually expired.
    self._current_time += self._EXPIRY_TIME / 2 + 1

    # Call with a different key and make sure we get a different port.
    self.assertEquals(self._pa.Get('test2'), cns._DEFAULT_CNS_PORT_RANGE[0] + 1)

    # After this sleep the port with key 'test' should expire on the next Get().
    self._current_time += self._EXPIRY_TIME / 2 + 1

    # Call with a different key and make sure we get the first port.
    self.assertEquals(self._pa.Get('test3'), cns._DEFAULT_CNS_PORT_RANGE[0])
    self.assertEquals(set(self._pa._ports.keys()), set([
        cns._DEFAULT_CNS_PORT_RANGE[0], cns._DEFAULT_CNS_PORT_RANGE[0] + 1]))


class ConstrainedNetworkServerTest(unittest.TestCase):
  """End to end tests for ConstrainedNetworkServer system."""

  # Amount of time to wait for the CNS to start up.
  _SERVER_START_SLEEP_SECS = 1

  # Sample data used to verify file serving.
  _TEST_DATA = 'The quick brown fox jumps over the lazy dog'

  # Server information.
  _SERVER_URL = ('http://127.0.0.1:%d/ServeConstrained?' %
                 cns._DEFAULT_SERVING_PORT)

  # Setting for latency testing.
  _LATENCY_TEST_SECS = 5

  def _StartServer(self):
    """Starts the CNS, returns pid."""
    cmd = ['python', 'cns.py']
    process = subprocess.Popen(cmd, stderr=subprocess.PIPE)

    # Wait for server to startup.
    line = True
    while line:
      line = process.stderr.readline()
      if 'STARTED' in line:
        return process.pid

    self.fail('Failed to start CNS.')

  def setUp(self):
    # Start the CNS.
    self._server_pid = self._StartServer()

    # Create temp file for serving. Run after server start so if a failure
    # during setUp() occurs we don't leave junk files around.
    f, self._file = tempfile.mkstemp(dir=os.getcwd())
    os.write(f, self._TEST_DATA)
    os.close(f)

    # Strip cwd off so we have a proper relative path.
    self._relative_fn = self._file[len(os.getcwd()) + 1:]

  def tearDown(self):
    os.unlink(self._file)
    os.kill(self._server_pid, signal.SIGKILL)

  def testServerServesFiles(self):
    now = time.time()

    f = urllib2.urlopen('%sf=%s' % (self._SERVER_URL, self._relative_fn))

    # Verify file data is served correctly.
    self.assertEqual(self._TEST_DATA, f.read())

    # For completeness ensure an unconstrained call takes less time than our
    # artificial constraints checked in the tests below.
    self.assertTrue(time.time() - now < self._LATENCY_TEST_SECS)

  def testServerLatencyConstraint(self):
    now = time.time()

    base_url = '%sf=%s' % (self._SERVER_URL, self._relative_fn)
    url = '%s&latency=%d' % (base_url, self._LATENCY_TEST_SECS * 1000)
    f = urllib2.urlopen(url)

    # Verify file data is served correctly.
    self.assertEqual(self._TEST_DATA, f.read())

    # Verify the request took longer than the requested latency.
    self.assertTrue(time.time() - now > self._LATENCY_TEST_SECS)

    # Verify the server properly redirected the URL.
    self.assertEquals(f.geturl(), base_url.replace(
        str(cns._DEFAULT_SERVING_PORT), str(cns._DEFAULT_CNS_PORT_RANGE[0])))


if __name__ == '__main__':
  unittest.main()
