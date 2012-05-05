# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Constrained network server (CNS) test base."""

import logging
import os
import Queue
import subprocess
import sys
import threading
import urllib2

import pyauto
import pyauto_paths

# List of commonly used network constraints settings.
# Each setting is a tuppe of the form:
#    ('TEST_NAME', [BANDWIDTH_Kbps, LATENCY_ms, PACKET_LOSS_%])
#
# Note: The test name should satisfy the regex [\w\.-]+ (check
# tools/perf_expectations/tests/perf_expectations_unittest.py for details). It
# is used to name the result graphs on the dashboards.
#
# The WiFi, DSL, and Cable settings were taken from webpagetest.org as
# approximations of their respective real world networks. The settings were
# based on 2011 FCC Broadband Data report (http://www.fcc.gov/document/
# measuring-broadband-america-report-consumer-broadband-performance-us).
DialUp = ('DialUp', [56, 120, 5])
Slow = ('Slow', [256, 105, 1])
Wifi = ('Wifi', [1024, 60, 0])
DSL = ('DSL', [1541, 50, 0])
Cable = ('Cable', [5120, 28, 0])
NoConstraints = ('NoConstraints', [0, 0, 0])

# Path to CNS executable relative to source root.
_CNS_PATH = os.path.join(
    'media', 'tools', 'constrained_network_server', 'cns.py')

# Port to start the CNS on.
_CNS_PORT = 9000

# Base CNS URL, only requires & separated parameter names appended.
CNS_BASE_URL = 'http://127.0.0.1:%d/ServeConstrained?' % _CNS_PORT

# Used for server sanity check.
_TEST_VIDEO = 'roller.webm'


class CNSTestBase(pyauto.PyUITest):
  """CNS test base hadles startup and teardown of CNS server."""

  def setUp(self):
    """Starts the Constrained Network Server (CNS)."""
    cmd = [sys.executable, os.path.join(pyauto_paths.GetSourceDir(), _CNS_PATH),
           '--port', str(_CNS_PORT),
           '--interface', 'lo',
           '--www-root', os.path.join(
               self.DataDir(), 'pyauto_private', 'media'),
           '-v',
           '--expiry-time', '0']

    self._cns_process = subprocess.Popen(cmd, stderr=subprocess.PIPE)
    ProcessLogger(self._cns_process)

    if self.WaitUntil(self._CanAccessServer, retry_sleep=3, timeout=30,
                      debug=False):
      pyauto.PyUITest.setUp(self)
    else:
      self.tearDown()
      self.fail('Failed to start CNS.')

  def _CanAccessServer(self):
    """Checks if the CNS server can serve a file with no network constraints."""
    test_url = ''.join([CNS_BASE_URL, 'f=', _TEST_VIDEO])
    try:
      return urllib2.urlopen(test_url) is not None
    except Exception:
      return False

  def tearDown(self):
    """Stops the Constrained Network Server (CNS)."""
    pyauto.PyUITest.tearDown(self)
    logging.debug('Stopping CNS server.')
    # Do not use process.kill(), it will not clean up cns.
    self.Kill(self._cns_process.pid)
    # Need to wait since the process logger has a lock on the process stderr.
    self._cns_process.wait()
    self.assertFalse(self._cns_process.returncode is None)
    logging.debug('CNS server stopped.')


class ProcessLogger(threading.Thread):
  """A thread to log a process's stderr output."""

  def __init__(self, process):
    """Starts the process logger thread.

    Args:
      process: The process to log.
    """
    threading.Thread.__init__(self)
    self._process = process
    self.start()

  def run(self):
    """Adds debug statements for the process's stderr output."""
    line = True
    while line:
      line = self._process.stderr.readline()
      logging.debug(line.strip())


def GetFileURL(file_name, bandwidth=0, latency=0, loss=0, new_port=False):
  """Returns CNS URL for the file with specified constraints.

  Args:
    Check cns.ServeConstrained() args for more details.
  """
  video_url = [CNS_BASE_URL, 'f=' + file_name]
  if bandwidth > 0:
    video_url.append('bandwidth=%d' % bandwidth)
  if latency > 0:
    video_url.append('latency=%d' % latency)
  if loss > 0:
    video_url.append('loss=%d' % loss)
  if new_port:
    video_url.append('new_port=%s' % new_port)
  return '&'.join(video_url)


def CreateCNSPerfTasks(network_constraints_settings, test_media_files):
  """Returns a queue of tasks combinining network constrains with media files.

  Args:
    network_constraints_settings: List of (setting_name, setting_values)
        tupples.
    test_media_files: List of media files to run the tests on.
  """
  # Convert relative test path into an absolute path.
  tasks = Queue.Queue()
  for file_name in test_media_files:
    for series_name, settings in network_constraints_settings:
      logging.debug('Add test: %s\tSettings: %s\tMedia: %s', series_name,
                    settings, file_name)
      tasks.put((series_name, settings, file_name))

  return tasks
