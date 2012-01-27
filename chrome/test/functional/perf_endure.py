#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance tests for Chrome Endure (long-running perf tests on Chrome).
"""

import logging
import os
import time

import perf
import perf_snapshot
import pyauto_functional  # Must be imported before pyauto.
import pyauto


class ChromeEndureBaseTest(perf.BasePerfTest):
  """Implements common functionality for all Chrome Endure tests.

  All Chrome Endure test classes should inherit from this class.
  """

  _DEFAULT_TEST_LENGTH_SEC = 60 * 60 * 12  # 12 hours.
  _GET_PERF_STATS_INTERVAL = 60 * 10  # Measure perf stats every 10 minutes.
  _ERROR_COUNT_THRESHOLD = 300  # Number of ChromeDriver errors to tolerate.

  def setUp(self):
    perf.BasePerfTest.setUp(self)

    self._test_length_sec = self._DEFAULT_TEST_LENGTH_SEC
    if 'TEST_LENGTH_SEC' in os.environ:
      self._test_length_sec = int(os.environ['TEST_LENGTH_SEC'])

    self._snapshotter = perf_snapshot.PerformanceSnapshotter()  # Tab index 0.
    self._dom_node_count_results = []
    self._event_listener_count_results = []
    self._process_private_mem_results = []
    self._test_start_time = 0

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Ensure Chrome enables remote debugging on port 9222.  This is required to
    # interact with Chrome's remote inspector.
    return (perf.BasePerfTest.ExtraChromeFlags(self) +
            ['--remote-debugging-port=9222'])

  def _GetProcessInfo(self, tab_title_substring):
    """Gets process info associated with an open tab.

    Args:
      tab_title_substring: A unique substring contained within the title of
                           the tab to use; needed for locating the tab info.

    Returns:
      A dictionary containing information about the specified tab process:
      {
        'private_mem': integer,  # Private memory associated with the tab
                                 # process, in KB.
      }
    """
    info = self.GetProcessInfo()
    tab_proc_info = []
    for browser_info in info['browsers']:
      for proc_info in browser_info['processes']:
        if (proc_info['child_process_type'] == 'Tab' and
            [x for x in proc_info['titles'] if tab_title_substring in x]):
          tab_proc_info.append(proc_info)
    self.assertEqual(len(tab_proc_info), 1,
                     msg='Expected to find 1 %s tab process, but found %d '
                         'instead.' % (tab_title_substring, len(tab_proc_info)))
    tab_proc_info = tab_proc_info[0]
    return {
      'private_mem': tab_proc_info['working_set_mem']['priv']
    }

  def _GetPerformanceStats(self, webapp_name, tab_title_substring):
    """Gets performance statistics and outputs the results.

    Args:
      webapp_name: A string name for the webapp being tested.  Should not
                   include spaces.  For example, 'Gmail', 'Docs', or 'Plus'.
      tab_title_substring: A unique substring contained within the title of
                           the tab to use, for identifying the appropriate tab.
    """
    logging.info('Gathering performance stats...')
    elapsed_time = time.time() - self._test_start_time
    elapsed_time = int(round(elapsed_time))

    memory_counts = self._snapshotter.GetMemoryObjectCounts()

    dom_node_count = memory_counts['DOMNodeCount']
    logging.info('  Total DOM node count: %d nodes' % dom_node_count)
    self._dom_node_count_results.append((elapsed_time, dom_node_count))

    event_listener_count = memory_counts['EventListenerCount']
    logging.info('  Event listener count: %d listeners' % event_listener_count)
    self._event_listener_count_results.append((elapsed_time,
                                               event_listener_count))

    proc_info = self._GetProcessInfo(tab_title_substring)
    logging.info('  Tab process private memory: %d KB' %
                 proc_info['private_mem'])
    self._process_private_mem_results.append((elapsed_time,
                                              proc_info['private_mem']))

    # Output the results seen so far, to be graphed.
    self._OutputPerfGraphValue(
        'TotalDOMNodeCount', self._dom_node_count_results, 'nodes',
        graph_name='%s-Nodes-DOM' % webapp_name, units_x='seconds')
    self._OutputPerfGraphValue(
        'EventListenerCount', self._event_listener_count_results, 'listeners',
        graph_name='%s-EventListeners' % webapp_name, units_x='seconds')
    self._OutputPerfGraphValue(
        'ProcessPrivateMemory', self._process_private_mem_results, 'KB',
        graph_name='%s-ProcessMemory-Private' % webapp_name, units_x='seconds')

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

  def _ClickElementByXpath(self, driver, wait, xpath):
    """Given the xpath for a DOM element, clicks on it using WebDriver.

    Args:
      driver: A WebDriver object, as returned by self.NewWebDriver().
      wait: A WebDriverWait object, as returned by WebDriverWait().
      xpath: The string xpath associated with the DOM element to click.

    Returns:
      True, if the DOM element was found and clicked successfully, or
      False, otherwise.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    try:
      element = wait.until(
          lambda _: self._GetElement(driver.find_element_by_xpath, xpath))
      element.click()
    except (selenium.common.exceptions.StaleElementReferenceException,
            selenium.common.exceptions.TimeoutException), e:
      logging.exception('WebDriver exception: %s' % e)
      return False
    return True

  def _WaitForElementByXpath(self, driver, wait, xpath):
    """Given the xpath for a DOM element, waits for it to exist in the DOM.

    Args:
      driver: A WebDriver object, as returned by self.NewWebDriver().
      wait: A WebDriverWait object, as returned by WebDriverWait().
      xpath: The string xpath associated with the DOM element for which to wait.

    Returns:
      True, if the DOM element was found in the DOM, or
      False, otherwise.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    try:
      wait.until(lambda _: self._GetElement(driver.find_element_by_xpath,
                                            xpath))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('WebDriver exception: %s' % str(e))
      return False
    return True


class ChromeEndureGmailTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Gmail."""

  _webapp_name = 'Gmail'
  _tab_title_substring = 'Gmail'

  def testGmailComposeDiscard(self):
    """Interact with Gmail while periodically gathering performance stats.

    This test continually composes/discards an e-mail using Gmail, and
    periodically gathers performance stats that may reveal memory bloat.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    from selenium.webdriver.support.ui import WebDriverWait

    # Log into a test Google account and open up Gmail.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://www.gmail.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(self._tab_title_substring in loaded_tab_title,
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

    # Interact with Gmail for the duration of the test.  Here, we repeat the
    # following sequence of interactions: click the "Compose" button, enter some
    # text into the "To" field, enter some text into the "Subject" field, then
    # click the "Discard" button to discard the message.
    self._test_start_time = time.time()
    last_perf_stats_time = time.time()
    self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)
    iteration_num = 0
    while time.time() - self._test_start_time < self._test_length_sec:
      iteration_num += 1

      if time.time() - last_perf_stats_time >= self._GET_PERF_STATS_INTERVAL:
        last_perf_stats_time = time.time()
        self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)

      if iteration_num % 10 == 0:
        remaining_time = self._test_length_sec - (
                             time.time() - self._test_start_time)
        logging.info('Chrome interaction #%d. Time remaining in test: %d sec.' %
                     (iteration_num, remaining_time))

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

    self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)


class ChromeEndureDocsTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Google Docs."""

  _webapp_name = 'Docs'
  _tab_title_substring = 'Docs'

  def testDocsAlternatelyClickLists(self):
    """Interact with Google Docs while periodically gathering performance stats.

    This test alternately clicks the "Owned by me" and "Home" buttons using
    Google Docs, and periodically gathers performance stats that may reveal
    memory bloat.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    from selenium.webdriver.support.ui import WebDriverWait

    driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    wait = WebDriverWait(driver, timeout=60)

    # Log into a test Google account and open up Google Docs.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://docs.google.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(self._tab_title_substring in loaded_tab_title,
                    msg='Loaded tab title does not contain "Docs": "%s"' %
                        loaded_tab_title)

    # Interact with Google Docs for the duration of the test.  Here, we repeat
    # the following sequence of interactions: click the "Owned by me" button,
    # then click the "Home" button.
    num_errors = 0
    self._test_start_time = time.time()
    last_perf_stats_time = time.time()
    self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)
    iteration_num = 0
    while time.time() - self._test_start_time < self._test_length_sec:
      iteration_num += 1

      if num_errors >= self._ERROR_COUNT_THRESHOLD:
        logging.error('Error count threshold (%d) reached. Terminating test '
                      'early.' % self._ERROR_COUNT_THRESHOLD)
        break

      if time.time() - last_perf_stats_time >= self._GET_PERF_STATS_INTERVAL:
        last_perf_stats_time = time.time()
        self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)

      if iteration_num % 10 == 0:
        remaining_time = self._test_length_sec - (
                             time.time() - self._test_start_time)
        logging.info('Chrome interaction #%d. Time remaining in test: %d sec.' %
                     (iteration_num, remaining_time))

      # Click the "Owned by me" button and wait for a resulting div to appear.
      if not self._ClickElementByXpath(
          driver, wait, '//div[text()="Owned by me"]'):
        num_errors += 1
      if not self._WaitForElementByXpath(
          driver, wait,
          '//div[@title="Owned by me filter.  Use backspace or delete to '
          'remove"]'):
        num_errors += 1
      time.sleep(1)

      # Click the "Home" button and wait for a resulting div to appear.
      if not self._ClickElementByXpath(driver, wait, '//div[text()="Home"]'):
        num_errors += 1
      if not self._WaitForElementByXpath(
          driver, wait,
          '//div[@title="Home filter.  Use backspace or delete to remove"]'):
        num_errors += 1
      time.sleep(1)

    self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)


class ChromeEndurePlusTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Google Plus."""

  _webapp_name = 'Plus'
  _tab_title_substring = 'Google+'

  def testPlusAlternatelyClickStreams(self):
    """Interact with Google Plus while periodically gathering performance stats.

    This test alternately clicks the "Friends" and "Acquaintances" buttons using
    Google Plus, and periodically gathers performance stats that may reveal
    memory bloat.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    from selenium.webdriver.support.ui import WebDriverWait

    driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    wait = WebDriverWait(driver, timeout=60)

    # Log into a test Google account and open up Google Plus.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://plus.google.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(self._tab_title_substring in loaded_tab_title,
                    msg='Loaded tab title does not contain "Google+": "%s"' %
                        loaded_tab_title)

    # Interact with Google Plus for the duration of the test.  Here, we repeat
    # the following sequence of interactions: click the "Friends" button, then
    # click the "Acquaintances" button.
    num_errors = 0
    self._test_start_time = time.time()
    last_perf_stats_time = time.time()
    self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)
    iteration_num = 0
    while time.time() - self._test_start_time < self._test_length_sec:
      iteration_num += 1

      if num_errors >= self._ERROR_COUNT_THRESHOLD:
        logging.error('Error count threshold (%d) reached. Terminating test '
                      'early.' % self._ERROR_COUNT_THRESHOLD)
        break

      if time.time() - last_perf_stats_time >= self._GET_PERF_STATS_INTERVAL:
        last_perf_stats_time = time.time()
        self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)

      if iteration_num % 10 == 0:
        remaining_time = self._test_length_sec - (
                             time.time() - self._test_start_time)
        logging.info('Chrome interaction #%d. Time remaining in test: %d sec.' %
                     (iteration_num, remaining_time))

      # Click the "Friends" button and wait for a resulting div to appear.
      if not self._ClickElementByXpath(
          driver, wait,
          '//a[text()="Friends" and starts-with(@href, "stream/circles")]'):
        num_errors += 1
      if not self._WaitForElementByXpath(
          driver, wait, '//div[text()="Friends"]'):
        num_errors += 1
      time.sleep(1)

      # Click the "Acquaintances" button and wait for a resulting div to appear.
      if not self._ClickElementByXpath(
          driver, wait,
          '//a[text()="Acquaintances" and '
          'starts-with(@href, "stream/circles")]'):
        num_errors += 1
      if not self._WaitForElementByXpath(
          driver, wait, '//div[text()="Acquaintances"]'):
        num_errors += 1
      time.sleep(1)

    self._GetPerformanceStats(self._webapp_name, self._tab_title_substring)


if __name__ == '__main__':
  pyauto_functional.Main()
