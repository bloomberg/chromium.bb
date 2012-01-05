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


class ChromeEndureGmailTest(perf.BasePerfTest):
  """Long-running performance tests for Chrome."""

  def setUp(self):
    perf.BasePerfTest.setUp(self)

    # Set up an object that takes v8 heap snapshots of the first opened tab
    # (index 0).
    self._snapshotter = perf_snapshot.PerformanceSnapshotter()
    self._snapshot_results = []

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Ensure Chrome enables remote debugging on port 9222.  This is required to
    # take v8 heap snapshots of tabs in Chrome.
    return (perf.BasePerfTest.ExtraChromeFlags(self) +
            ['--remote-debugging-port=9222'])

  def _TakeHeapSnapshot(self):
    """Takes a v8 heap snapshot using |self._snapshotter| and stores the result.

    This function will fail the current test if no snapshot can be taken.

    Returns:
      The number of seconds it took to take the heap snapshot.
    """
    start_time = time.time()
    snapshot = self._snapshotter.HeapSnapshot()
    elapsed_time = time.time() - start_time
    self.assertTrue(snapshot, msg='Failed to take a v8 heap snapshot.')
    self._snapshot_results.append(snapshot[0])
    return elapsed_time

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

    def _GetElement(find_by, value):
      """Gets a WebDriver element object from the webpage DOM.

      Args:
        find_by: A callable that queries WebDriver for an element from the DOM.
        value: A string value that can be passed to the |find_by| callable.

      Returns:
        The identified WebDriver element object, if found in the DOM, or
        None, otherwise.
      """
      try:
        return find_by(value)
      except selenium.common.exceptions.NoSuchElementException:
        return None

    # Wait until Gmail's 'canvas_frame' loads and the 'Inbox' link is present.
    # TODO(dennisjeffrey): Check with the Gmail team to see if there's a better
    # way to tell when the webpage is ready for user interaction.
    wait.until(_SwitchToCanvasFrame)  # Raises exception if the timeout is hit.
    # Wait for the inbox to appear.
    wait.until(lambda _: _GetElement(
                   driver.find_element_by_partial_link_text, 'Inbox'))

    # Interact with Gmail for awhile.  Here, we repeat the following sequence of
    # interactions: click the "Compose" button, enter some text into the "To"
    # field, enter some text into the "Subject" field, then click the "Discard"
    # button to discard the message.
    heap_size_results = []
    node_count_results = []
    snapshot_iteration = 0
    num_iterations = 501
    for i in xrange(num_iterations):
      logging.info('Chrome interaction iteration %d of %d.' % (
                   i + 1, num_iterations))

      compose_button = wait.until(lambda _: _GetElement(
                                      driver.find_element_by_xpath,
                                      '//div[text()="COMPOSE"]'))
      compose_button.click()

      to_field = wait.until(lambda _: _GetElement(
                                driver.find_element_by_name, 'to'))
      to_field.send_keys('nobody@nowhere.com')

      subject_field = wait.until(lambda _: _GetElement(
                                     driver.find_element_by_name, 'subject'))
      subject_field.send_keys('This message is about to be discarded')

      discard_button = wait.until(lambda _: _GetElement(
                                      driver.find_element_by_xpath,
                                      '//div[text()="Discard"]'))
      discard_button.click()

      # Wait for the message to be discarded, assumed to be true after the
      # "To" field is removed from the webpage DOM.
      wait.until(lambda _: not _GetElement(
                     driver.find_element_by_name, 'to'))

      # Snapshot after the first iteration, then every 50 iterations after that.
      if i % 50 == 0:
        logging.info('Taking heap snapshot...')
        sec_to_snapshot = self._TakeHeapSnapshot()
        logging.info('Snapshot taken (%.2f sec).' % sec_to_snapshot)

        assert len(self._snapshot_results) >= 1
        base_timestamp = self._snapshot_results[0]['timestamp']

        snapshot_info = self._snapshot_results[-1]  # Get the last snapshot.
        logging.info('Snapshot time: %.2f sec' % (
                     snapshot_info['timestamp'] - base_timestamp))
        heap_size = snapshot_info['total_heap_size'] / (1024.0 * 1024.0)
        logging.info('  Total heap size: %.2f MB' % heap_size)
        heap_size_results.append((snapshot_iteration, heap_size))

        node_count = snapshot_info['total_node_count']
        logging.info('  Total node count: %d nodes' % node_count)
        node_count_results.append((snapshot_iteration, node_count))
        snapshot_iteration += 1
        self._OutputPerfGraphValue(
            'HeapSize', heap_size_results, 'MB', graph_name='Gmail-Heap',
            units_x='iteration')
        self._OutputPerfGraphValue(
            'TotalNodeCount', node_count_results, 'nodes',
            graph_name='Gmail-Nodes', units_x='iteration')

    # Output other snapshot results.
    assert len(self._snapshot_results) >= 1
    max_heap_size = 0
    max_node_count = 0
    for index, snapshot_info in enumerate(self._snapshot_results):
      heap_size = snapshot_info['total_heap_size'] / (1024.0 * 1024.0)
      if heap_size > max_heap_size:
        max_heap_size = heap_size
      node_count = snapshot_info['total_node_count']
      if node_count > max_node_count:
        max_node_count = node_count
    self._OutputPerfGraphValue(
        'HeapSize', max_heap_size, 'MB', graph_name='Gmail-Heap-Max')
    self._OutputPerfGraphValue(
        'TotalNodeCount', max_node_count, 'nodes', graph_name='Gmail-Nodes-Max')


if __name__ == '__main__':
  pyauto_functional.Main()
