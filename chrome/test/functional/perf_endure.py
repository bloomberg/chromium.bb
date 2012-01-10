#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance tests for Chrome Endure (long-running perf tests on Chrome).
"""

import logging
import time

import perf
import perf_snapshot
import pyauto_functional  # Must be imported before pyauto.
import pyauto


class ChromeEndureBaseTest(perf.BasePerfTest):
  """Implements common functionality for all Chrome Endure tests.

  All Chrome Endure test classes should inherit from this class.
  """

  def setUp(self):
    perf.BasePerfTest.setUp(self)

    # Set up an object that takes v8 heap snapshots of the first opened tab
    # (index 0).
    self._snapshotter = perf_snapshot.PerformanceSnapshotter()
    self._full_snapshot_results = []
    self._snapshot_iteration = 0
    self._heap_size_results = []
    self._v8_node_count_results = []
    self._dom_node_count_results = []
    self._event_listener_count_results = []

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Ensure Chrome enables remote debugging on port 9222.  This is required to
    # take v8 heap snapshots of tabs in Chrome.
    return (perf.BasePerfTest.ExtraChromeFlags(self) +
            ['--remote-debugging-port=9222'])

  def _TakeHeapSnapshot(self, webapp_name):
    """Takes a v8 heap snapshot and outputs/stores the results.

    This function will fail the current test if no snapshot can be taken.  A
    snapshot stored by this function is represented by a dictionary with the
    following format:
    {
      'url': string,  # URL of the webpage that was snapshotted.
      'timestamp': float,  # Time when snapshot taken (seconds since epoch).
      'total_v8_node_count': integer,  # Total number of nodes in the v8 heap.
      'total_heap_size': integer,  # Total heap size (number of bytes).
      'total_dom_node_count': integer, # Total number of DOM nodes.
      'event_listener_count': integer, # Total number of event listeners.
    }

    Args:
      webapp_name: A string name for the webapp being testing.  Should not
                   include spaces.  For example, 'Gmail', 'Docs', or 'Plus'.
    """
    # Take the snapshot and store the associated information.
    logging.info('Taking v8 heap snapshot...')
    start_time = time.time()
    snapshot = self._snapshotter.HeapSnapshot()
    elapsed_time = time.time() - start_time
    self.assertTrue(snapshot, msg='Failed to take a v8 heap snapshot.')
    snapshot_info = snapshot[0]

    # Collect other count information to add to the snapshot data.
    memory_counts = self._snapshotter.GetMemoryObjectCounts()
    snapshot_info['total_dom_node_count'] = memory_counts['DOMNodeCount']
    snapshot_info['event_listener_count'] = memory_counts['EventListenerCount']

    self._full_snapshot_results.append(snapshot_info)
    logging.info('Snapshot taken (%.2f sec).' % elapsed_time)

    base_timestamp = self._full_snapshot_results[0]['timestamp']

    logging.info('Snapshot time: %.2f sec' % (
                 snapshot_info['timestamp'] - base_timestamp))
    heap_size = snapshot_info['total_heap_size'] / (1024.0 * 1024.0)
    logging.info('  Total heap size: %.2f MB' % heap_size)
    self._heap_size_results.append((self._snapshot_iteration, heap_size))

    v8_node_count = snapshot_info['total_v8_node_count']
    logging.info('  Total v8 node count: %d nodes' % v8_node_count)
    self._v8_node_count_results.append((self._snapshot_iteration,
                                        v8_node_count))

    dom_node_count = snapshot_info['total_dom_node_count']
    logging.info('  Total DOM node count: %d nodes' % dom_node_count)
    self._dom_node_count_results.append((self._snapshot_iteration,
                                         dom_node_count))

    event_listener_count = snapshot_info['event_listener_count']
    logging.info('  Event listener count: %d listeners' % event_listener_count)
    self._event_listener_count_results.append((self._snapshot_iteration,
                                               event_listener_count))

    self._snapshot_iteration += 1

    # Output the results seen so far, to be graphed.
    self._OutputPerfGraphValue(
        'HeapSize', self._heap_size_results, 'MB',
        graph_name='%s-Heap' % webapp_name, units_x='iteration')
    self._OutputPerfGraphValue(
        'TotalV8NodeCount', self._v8_node_count_results, 'nodes',
        graph_name='%s-Nodes-V8' % webapp_name, units_x='iteration')
    self._OutputPerfGraphValue(
        'TotalDOMNodeCount', self._dom_node_count_results, 'nodes',
        graph_name='%s-Nodes-DOM' % webapp_name, units_x='iteration')
    self._OutputPerfGraphValue(
        'EventListenerCount', self._event_listener_count_results, 'listeners',
        graph_name='%s-EventListeners' % webapp_name, units_x='iteration')

  def _OutputFinalHeapSnapshotResults(self, webapp_name):
    """Outputs final snapshot results to be graphed at the end of a test.

    Assumes that at least one snapshot was previously taken by the current test.

    Args:
      webapp_name: A string name for the webapp being testing.  Should not
                   include spaces.  For example, 'Gmail', 'Docs', or 'Plus'.
    """
    assert len(self._full_snapshot_results) >= 1
    max_heap_size = 0
    max_v8_node_count = 0
    max_dom_node_count = 0
    max_event_listener_count = 0
    for index, snapshot_info in enumerate(self._full_snapshot_results):
      heap_size = snapshot_info['total_heap_size'] / (1024.0 * 1024.0)
      if heap_size > max_heap_size:
        max_heap_size = heap_size
      v8_node_count = snapshot_info['total_v8_node_count']
      if v8_node_count > max_v8_node_count:
        max_v8_node_count = v8_node_count
      dom_node_count = snapshot_info['total_dom_node_count']
      if dom_node_count > max_dom_node_count:
        max_dom_node_count = dom_node_count
      event_listener_count = snapshot_info['event_listener_count']
      if event_listener_count > max_event_listener_count:
        max_event_listener_count = event_listener_count
    self._OutputPerfGraphValue(
        'MaxHeapSize', max_heap_size, 'MB',
        graph_name='%s-Heap-Max' % webapp_name)
    self._OutputPerfGraphValue(
        'MaxV8NodeCount', max_v8_node_count, 'nodes',
        graph_name='%s-Nodes-V8-Max' % webapp_name)
    self._OutputPerfGraphValue(
        'MaxDOMNodeCount', max_dom_node_count, 'nodes',
        graph_name='%s-Nodes-DOM-Max' % webapp_name)
    self._OutputPerfGraphValue(
        'MaxEventListenerCount', max_event_listener_count, 'listeners',
        graph_name='%s-EventListeners-Max' % webapp_name)

  def _GetElement(self, find_by, value):
    """Gets a WebDriver element object from the webpage DOM.

    Args:
      find_by: A callable that queries WebDriver for an element from the DOM.
      value: A string value that can be passed to the |find_by| callable.

    Returns:
      The identified WebDriver element object, if found in the DOM, or
      None, otherwise.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    try:
      return find_by(value)
    except selenium.common.exceptions.NoSuchElementException:
      return None


class ChromeEndureGmailTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Gmail."""

  def testGmailComposeDiscard(self):
    """Interact with Gmail while periodically taking v8 heap snapshots.

    This test continually composes/discards an e-mail using Gmail, and
    periodically takes v8 heap snapshots that may reveal memory bloat.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    from selenium.webdriver.support.ui import WebDriverWait

    # Log into a test Google account and open up Gmail.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://www.gmail.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue('Gmail' in loaded_tab_title,
                    msg='Loaded tab title does not contain "Gmail": "%s"' %
                        loaded_tab_title)

    driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    wait = WebDriverWait(driver, timeout=60)

    def _SwitchToCanvasFrame(driver):
      """Switch the WebDriver to Gmail's 'canvas_frame', if it's available.

      Args:
        driver: A selenium.webdriver.remote.webdriver.WebDriver object.

      Returns:
        True, if the switch to Gmail's 'canvas_frame' is successful, or
        False if not.
      """
      try:
        driver.switch_to_frame('canvas_frame')
        return True
      except selenium.common.exceptions.NoSuchFrameException:
        return False

    # Wait until Gmail's 'canvas_frame' loads and the 'Inbox' link is present.
    # TODO(dennisjeffrey): Check with the Gmail team to see if there's a better
    # way to tell when the webpage is ready for user interaction.
    wait.until(_SwitchToCanvasFrame)  # Raises exception if the timeout is hit.
    # Wait for the inbox to appear.
    wait.until(lambda _: self._GetElement(
                   driver.find_element_by_partial_link_text, 'Inbox'))

    # Interact with Gmail for a while.  Here, we repeat the following sequence
    # of interactions: click the "Compose" button, enter some text into the "To"
    # field, enter some text into the "Subject" field, then click the "Discard"
    # button to discard the message.
    num_iterations = 501
    for i in xrange(num_iterations):
      logging.info('Chrome interaction iteration %d of %d.' % (
                   i + 1, num_iterations))

      compose_button = wait.until(lambda _: self._GetElement(
                                      driver.find_element_by_xpath,
                                      '//div[text()="COMPOSE"]'))
      compose_button.click()

      to_field = wait.until(lambda _: self._GetElement(
                                driver.find_element_by_name, 'to'))
      to_field.send_keys('nobody@nowhere.com')

      subject_field = wait.until(lambda _: self._GetElement(
                                     driver.find_element_by_name, 'subject'))
      subject_field.send_keys('This message is about to be discarded')

      discard_button = wait.until(lambda _: self._GetElement(
                                      driver.find_element_by_xpath,
                                      '//div[text()="Discard"]'))
      discard_button.click()

      # Wait for the message to be discarded, assumed to be true after the
      # "To" field is removed from the webpage DOM.
      wait.until(lambda _: not self._GetElement(
                     driver.find_element_by_name, 'to'))

      # Snapshot after the first iteration, then every 50 iterations after that.
      if i % 50 == 0:
        self._TakeHeapSnapshot('Gmail')

    self._OutputFinalHeapSnapshotResults('Gmail')


class ChromeEndureDocsTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Google Docs."""

  def testDocsAlternatelyClickLists(self):
    """Interact with Google Docs while periodically taking v8 heap snapshots.

    This test alternately clicks the "Owned by me" and "Home" buttons using
    Google Docs, and periodically takes v8 heap snapshots that may reveal memory
    bloat.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    from selenium.webdriver.support.ui import WebDriverWait

    # Log into a test Google account and open up Google Docs.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://docs.google.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue('Docs' in loaded_tab_title,
                    msg='Loaded tab title does not contain "Docs": "%s"' %
                        loaded_tab_title)

    driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    wait = WebDriverWait(driver, timeout=60)

    # Interact with Google Docs for a while.  Here, we repeat the following
    # sequence of interactions: click the "Owned by me" button, then click the
    # "Home" button.
    num_iterations = 2001
    for i in xrange(num_iterations):
      if i % 5 == 0:
        logging.info('Chrome interaction iteration %d of %d.' % (
                     i + 1, num_iterations))

      # Click the "Owned by me" button and wait for a resulting div to appear.
      owned_by_me_button = wait.until(lambda _: self._GetElement(
                                          driver.find_element_by_xpath,
                                          '//div[text()="Owned by me"]'))
      owned_by_me_button.click()
      wait.until(
          lambda _: self._GetElement(
              driver.find_element_by_xpath,
              '//div[@title="Owned by me filter.  Use backspace or delete to '
              'remove"]'))
      time.sleep(0.1)

      # Click the "Home" button and wait for a resulting div to appear.
      home_button = wait.until(lambda _: self._GetElement(
                                   driver.find_element_by_xpath,
                                   '//div[text()="Home"]'))
      home_button.click()
      wait.until(
          lambda _: self._GetElement(
              driver.find_element_by_xpath,
              '//div[@title="Home filter.  Use backspace or delete to '
              'remove"]'))
      time.sleep(0.1)

      # Snapshot after the first iteration, then every 100 iterations after
      # that.
      if i % 100 == 0:
        self._TakeHeapSnapshot('Docs')

    self._OutputFinalHeapSnapshotResults('Docs')


class ChromeEndurePlusTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Google Plus."""

  def testPlusAlternatelyClickStreams(self):
    """Interact with Google Plus while periodically taking v8 heap snapshots.

    This test alternately clicks the "Friends" and "Acquaintances" buttons using
    Google Plus, and periodically takes v8 heap snapshots that may reveal memory
    bloat.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    from selenium.webdriver.support.ui import WebDriverWait

    # Log into a test Google account and open up Google Plus.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://plus.google.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue('Google+' in loaded_tab_title,
                    msg='Loaded tab title does not contain "Google+": "%s"' %
                        loaded_tab_title)

    driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    wait = WebDriverWait(driver, timeout=60)

    # Interact with Google Plus for a while.  Here, we repeat the following
    # sequence of interactions: click the "Friends" button, then click the
    # "Acquaintances" button.
    num_iterations = 1001
    for i in xrange(num_iterations):
      if i % 5 == 0:
        logging.info('Chrome interaction iteration %d of %d.' % (
                     i + 1, num_iterations))

      # Click the "Friends" button and wait for a resulting div to appear.
      friends_button = wait.until(lambda _: self._GetElement(
                                      driver.find_element_by_xpath,
                                      '//a[text()="Friends"]'))
      friends_button.click()
      wait.until(lambda _: self._GetElement(
                     driver.find_element_by_xpath,
                     '//div[text()="Friends"]'))
      time.sleep(0.1)

      # Click the "Acquaintances" button and wait for a resulting div to appear.
      acquaintances_button = wait.until(lambda _: self._GetElement(
                                            driver.find_element_by_xpath,
                                            '//a[text()="Acquaintances"]'))
      acquaintances_button.click()
      wait.until(lambda _: self._GetElement(
                     driver.find_element_by_xpath,
                     '//div[text()="Acquaintances"]'))
      time.sleep(0.1)

      # Snapshot after the first iteration, then every 100 iterations after
      # that.
      if i % 100 == 0:
        self._TakeHeapSnapshot('Plus')

    self._OutputFinalHeapSnapshotResults('Plus')


if __name__ == '__main__':
  pyauto_functional.Main()
