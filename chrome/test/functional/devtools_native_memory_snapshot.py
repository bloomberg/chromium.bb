#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from urlparse import urlparse

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils
import remote_inspector_client
import webpagereplay


class DevToolsNativeMemorySnapshotTest(pyauto.PyUITest):
  """Test for tracking unknown share in the DevTools native memory snapshots.

  This test navigates the browser to a test page, then takes native memory
  snapshot over remote debugging protocol and prints render process private
  memory size and unknown size extracted from the snapshot. It is used to
  track size of the memory that is not counted by DevTools memory
  instrumentation.

  The test uses Web Page Replay server as a proxy that allows to replay
  the same state of the test pages and avoid heavy network traffic on the
  real web sites. See webpagereplay.ReplayServer documentation to learn how
  to record new page archives.
  """

  # DevTools test pages live in src/data/devtools rather than
  # src/chrome/test/data
  DATA_PATH = os.path.abspath(
      os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
                   'data', 'devtools_test_pages'))

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Ensure Chrome enables remote debugging on port 9222.  This is required to
    # interact with Chrome's remote inspector.
    extra_flags = ['--remote-debugging-port=9222'] + webpagereplay.CHROME_FLAGS
    return (pyauto.PyUITest.ExtraChromeFlags(self) + extra_flags)

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    # Set up a remote inspector client associated with tab 0.
    logging.info('Setting up connection to remote inspector...')
    self._remote_inspector_client = (
        remote_inspector_client.RemoteInspectorClient())
    logging.info('Connection to remote inspector set up successfully.')

  def tearDown(self):
    logging.info('Terminating connection to remote inspector...')
    self._remote_inspector_client.Stop()
    logging.info('Connection to remote inspector terminated.')
    super(DevToolsNativeMemorySnapshotTest, self).tearDown()

  def testNytimes(self):
    self._RunTestWithUrl('http://www.nytimes.com/')

  def testCnn(self):
    self._RunTestWithUrl('http://www.cnn.com/')

  def testGoogle(self):
    self._RunTestWithUrl('http://www.google.com/')

  def _RunTestWithUrl(self, url):
    """Dumps native memory snapshot data for given page."""
    replay_options = None
    hostname = urlparse(url).hostname
    archive_path = os.path.join(self.DATA_PATH, hostname + '.wpr')
    with webpagereplay.ReplayServer(archive_path, replay_options):
      self.NavigateToURL(url)
    snapshot = self._remote_inspector_client.GetProcessMemoryDistribution()
    total = snapshot.GetProcessPrivateMemorySize()
    unknown = snapshot.GetUnknownSize()
    logging.info('Got data for url: %s, total size = %d, unknown size = %d '%
        (url, total, unknown))

    graph_name = 'DevTools Native Snapshot - ' + hostname
    pyauto_utils.PrintPerfResult(graph_name, 'Total', total, 'bytes')
    pyauto_utils.PrintPerfResult(graph_name, 'Unknown', unknown, 'bytes')


if __name__ == '__main__':
  pyauto_functional.Main()
