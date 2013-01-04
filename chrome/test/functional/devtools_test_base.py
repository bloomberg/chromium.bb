#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import tempfile
from urlparse import urlparse

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils
import remote_inspector_client
import webpagereplay


class DevToolsTestBase(pyauto.PyUITest):
  """Base class for DevTools tests that use replay server.

  This class allows to navigate the browser to a test page and take native
  memory snapshot over remote debugging protocol.

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

  WEBPAGEREPLAY_HOST = '127.0.0.1'
  WEBPAGEREPLAY_HTTP_PORT = 8080
  WEBPAGEREPLAY_HTTPS_PORT = 8413

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Ensure Chrome enables remote debugging on port 9222.  This is required to
    # interact with Chrome's remote inspector.
    extra_flags = ['--remote-debugging-port=9222'] + \
        webpagereplay.GetChromeFlags(self.WEBPAGEREPLAY_HOST,
                                     self.WEBPAGEREPLAY_HTTP_PORT,
                                     self.WEBPAGEREPLAY_HTTPS_PORT)
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
    super(DevToolsTestBase, self).tearDown()

  def RunTestWithUrl(self, url):
    """Navigates browser to given URL and takes native memory snapshot."""
    replay_options = None
    hostname = urlparse(url).hostname
    archive_path = os.path.join(self.DATA_PATH, hostname + '.wpr')
    with webpagereplay.ReplayServer(archive_path,
                                    self.WEBPAGEREPLAY_HOST,
                                    self.WEBPAGEREPLAY_HTTP_PORT,
                                    self.WEBPAGEREPLAY_HTTPS_PORT,
                                    replay_options):
      self.NavigateToURL(url)
    snapshot = self._remote_inspector_client.GetProcessMemoryDistribution()
    logging.info('Got snapshot for url: %s' % url)
    self.PrintTestResult(hostname, snapshot)

  def PrintTestResult(self, hostname, snapshot):
    """This method is supposed to be overriden in subclasses."""
    pass

