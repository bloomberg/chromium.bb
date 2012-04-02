#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance tests for Chrome Endure (long-running perf tests on Chrome).

This module accepts the following environment variable inputs:
  TEST_LENGTH: The number of seconds in which to run each test.
  PERF_STATS_INTERVAL: The number of seconds to wait in-between each sampling
      of performance/memory statistics.
"""

import logging
import os
import re
import time

import perf
import pyauto_functional  # Must be imported before pyauto.
import pyauto
import remote_inspector_client
import selenium.common.exceptions
from selenium.webdriver.support.ui import WebDriverWait


class ChromeEndureBaseTest(perf.BasePerfTest):
  """Implements common functionality for all Chrome Endure tests.

  All Chrome Endure test classes should inherit from this class.
  """

  _DEFAULT_TEST_LENGTH_SEC = 60 * 60 * 6  # Tests run for 6 hours.
  _GET_PERF_STATS_INTERVAL = 60 * 10  # Measure perf stats every 10 minutes.
  _ERROR_COUNT_THRESHOLD = 300  # Number of ChromeDriver errors to tolerate.

  def setUp(self):
    perf.BasePerfTest.setUp(self)

    self._test_length_sec = int(
        os.environ.get('TEST_LENGTH', self._DEFAULT_TEST_LENGTH_SEC))
    self._get_perf_stats_interval = int(
        os.environ.get('PERF_STATS_INTERVAL', self._GET_PERF_STATS_INTERVAL))

    logging.info('Running test for %d seconds.', self._test_length_sec)
    logging.info('Gathering perf stats every %d seconds.',
                 self._get_perf_stats_interval)

    # Set up a remote inspector client associated with tab 0.
    logging.info('Setting up connection to remote inspector...')
    self._remote_inspector_client = (
        remote_inspector_client.RemoteInspectorClient())
    logging.info('Connection to remote inspector set up successfully.')

    self._dom_node_count_results = []
    self._event_listener_count_results = []
    self._browser_process_private_mem_results = []
    self._tab_process_private_mem_results = []
    self._v8_mem_used_results = []
    self._test_start_time = 0
    self._num_errors = 0
    self._iteration_num = 0

  def tearDown(self):
    logging.info('Terminating connection to remote inspector...')
    self._remote_inspector_client.Stop()
    logging.info('Connection to remote inspector terminated.')
    perf.BasePerfTest.tearDown(self)  # Must be done at end of this function.

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Ensure Chrome enables remote debugging on port 9222.  This is required to
    # interact with Chrome's remote inspector.
    return (perf.BasePerfTest.ExtraChromeFlags(self) +
            ['--remote-debugging-port=9222'])

  def _RunControlTest(self, webapp_name, tab_title_substring, test_description,
                      do_scenario):
    """The main test harness function to run a general control test.

    After a test has performed any setup work and has navigated to the proper
    starting webpage, this function should be invoked to run the endurance test.

    Args:
      webapp_name: A string name for the webapp being tested.  Should not
          include spaces.  For example, 'Gmail', 'Docs', or 'Plus'.
      tab_title_substring: A unique substring contained within the title of
          the tab to use, for identifying the appropriate tab.
      test_description: A string description of what the test does, used for
          outputting results to be graphed.  Should not contain spaces.  For
          example, 'ComposeDiscard' for Gmail.
      do_scenario: A callable to be invoked that implements the scenario to be
          performed by this test.  The callable is invoked iteratively for the
          duration of the test.
    """
    self._num_errors = 0
    self._test_start_time = time.time()
    last_perf_stats_time = time.time()
    self._GetPerformanceStats(
        webapp_name, test_description, tab_title_substring)
    self._iteration_num = 0
    while time.time() - self._test_start_time < self._test_length_sec:
      self._iteration_num += 1

      if self._num_errors >= self._ERROR_COUNT_THRESHOLD:
        logging.error('Error count threshold (%d) reached. Terminating test '
                      'early.' % self._ERROR_COUNT_THRESHOLD)
        break

      if time.time() - last_perf_stats_time >= self._get_perf_stats_interval:
        last_perf_stats_time = time.time()
        self._GetPerformanceStats(
            webapp_name, test_description, tab_title_substring)

      if self._iteration_num % 10 == 0:
        remaining_time = self._test_length_sec - (time.time() -
                                                  self._test_start_time)
        logging.info('Chrome interaction #%d. Time remaining in test: %d sec.' %
                     (self._iteration_num, remaining_time))

      do_scenario()

    self._GetPerformanceStats(
        webapp_name, test_description, tab_title_substring)

  def _GetProcessInfo(self, tab_title_substring):
    """Gets process info associated with an open browser/tab.

    Args:
      tab_title_substring: A unique substring contained within the title of
          the tab to use; needed for locating the tab info.

    Returns:
      A dictionary containing information about the browser and specified tab
      process:
      {
        'browser_private_mem': integer,  # Private memory associated with the
                                         # browser process, in KB.
        'tab_private_mem': integer,  # Private memory associated with the tab
                                     # process, in KB.
      }
    """
    browser_process_name = (
        self.GetBrowserInfo()['properties']['BrowserProcessExecutableName'])
    info = self.GetProcessInfo()

    # Get the information associated with the browser process.
    browser_proc_info = []
    for browser_info in info['browsers']:
      if browser_info['process_name'] == browser_process_name:
        for proc_info in browser_info['processes']:
          if proc_info['child_process_type'] == 'Browser':
            browser_proc_info.append(proc_info)
    self.assertEqual(len(browser_proc_info), 1,
                     msg='Expected to find 1 Chrome browser process, but found '
                         '%d instead.\nCurrent process info:\n%s.' % (
                         len(browser_proc_info), self.pformat(info)))

    # Get the process information associated with the specified tab.
    tab_proc_info = []
    for browser_info in info['browsers']:
      for proc_info in browser_info['processes']:
        if (proc_info['child_process_type'] == 'Tab' and
            [x for x in proc_info['titles'] if tab_title_substring in x]):
          tab_proc_info.append(proc_info)
    self.assertEqual(len(tab_proc_info), 1,
                     msg='Expected to find 1 %s tab process, but found %d '
                         'instead.\nCurrent process info:\n%s.' % (
                         tab_title_substring, len(tab_proc_info),
                         self.pformat(info)))

    browser_proc_info = browser_proc_info[0]
    tab_proc_info = tab_proc_info[0]
    return {
      'browser_private_mem': browser_proc_info['working_set_mem']['priv'],
      'tab_private_mem': tab_proc_info['working_set_mem']['priv'],
    }

  def _GetPerformanceStats(self, webapp_name, test_description,
                           tab_title_substring):
    """Gets performance statistics and outputs the results.

    Args:
      webapp_name: A string name for the webapp being tested.  Should not
          include spaces.  For example, 'Gmail', 'Docs', or 'Plus'.
      test_description: A string description of what the test does, used for
          outputting results to be graphed.  Should not contain spaces.  For
          example, 'ComposeDiscard' for Gmail.
      tab_title_substring: A unique substring contained within the title of
          the tab to use, for identifying the appropriate tab.
    """
    logging.info('Gathering performance stats...')
    elapsed_time = time.time() - self._test_start_time
    elapsed_time = int(round(elapsed_time))

    memory_counts = self._remote_inspector_client.GetMemoryObjectCounts()

    dom_node_count = memory_counts['DOMNodeCount']
    logging.info('  Total DOM node count: %d nodes' % dom_node_count)
    self._dom_node_count_results.append((elapsed_time, dom_node_count))

    event_listener_count = memory_counts['EventListenerCount']
    logging.info('  Event listener count: %d listeners' % event_listener_count)
    self._event_listener_count_results.append((elapsed_time,
                                               event_listener_count))

    proc_info = self._GetProcessInfo(tab_title_substring)
    logging.info('  Browser process private memory: %d KB' %
                 proc_info['browser_private_mem'])
    self._browser_process_private_mem_results.append(
        (elapsed_time, proc_info['browser_private_mem']))
    logging.info('  Tab process private memory: %d KB' %
                 proc_info['tab_private_mem'])
    self._tab_process_private_mem_results.append(
        (elapsed_time, proc_info['tab_private_mem']))

    v8_info = self.GetV8HeapStats()  # First window, first tab.
    v8_mem_used = v8_info['v8_memory_used'] / 1024.0  # Convert to KB.
    logging.info('  V8 memory used: %f KB' % v8_mem_used)
    self._v8_mem_used_results.append((elapsed_time, v8_mem_used))

    # Output the results seen so far, to be graphed.
    self._OutputPerfGraphValue(
        'TotalDOMNodeCount', self._dom_node_count_results, 'nodes',
        graph_name='%s%s-Nodes-DOM' % (webapp_name, test_description),
        units_x='seconds')
    self._OutputPerfGraphValue(
        'EventListenerCount', self._event_listener_count_results, 'listeners',
        graph_name='%s%s-EventListeners' % (webapp_name, test_description),
        units_x='seconds')
    self._OutputPerfGraphValue(
        'BrowserPrivateMemory', self._browser_process_private_mem_results, 'KB',
        graph_name='%s%s-BrowserMem-Private' % (webapp_name, test_description),
        units_x='seconds')
    self._OutputPerfGraphValue(
        'TabPrivateMemory', self._tab_process_private_mem_results, 'KB',
        graph_name='%s%s-TabMem-Private' % (webapp_name, test_description),
        units_x='seconds')
    self._OutputPerfGraphValue(
        'V8MemoryUsed', self._v8_mem_used_results, 'KB',
        graph_name='%s%s-V8MemUsed' % (webapp_name, test_description),
        units_x='seconds')

  def _GetElement(self, find_by, value):
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
    try:
      wait.until(lambda _: self._GetElement(driver.find_element_by_xpath,
                                            xpath))
    except selenium.common.exceptions.TimeoutException, e:
      logging.exception('WebDriver exception: %s' % str(e))
      return False
    return True


class ChromeEndureControlTest(ChromeEndureBaseTest):
  """Control tests for Chrome Endure."""

  _webapp_name = 'Control'
  _tab_title_substring = 'Chrome Endure Control Test'

  def testControlAttachDetachDOMTree(self):
    """Continually attach and detach a DOM tree from a basic document."""
    test_description = 'AttachDetachDOMTree'
    url = self.GetHttpURLForDataPath('chrome_endure', 'endurance_control.html')
    self.NavigateToURL(url)
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(self._tab_title_substring in loaded_tab_title,
                    msg='Loaded tab title does not contain "%s": "%s"' %
                        (self._tab_title_substring, loaded_tab_title))

    def scenario():
      # Just sleep.  Javascript in the webpage itself does the work.
      time.sleep(5)

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, scenario)

  def testControlAttachDetachDOMTreeWebDriver(self):
    """Use WebDriver to attach and detach a DOM tree from a basic document."""
    test_description = 'AttachDetachDOMTreeWebDriver'
    url = self.GetHttpURLForDataPath('chrome_endure',
                                     'endurance_control_webdriver.html')
    self.NavigateToURL(url)
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(self._tab_title_substring in loaded_tab_title,
                    msg='Loaded tab title does not contain "%s": "%s"' %
                        (self._tab_title_substring, loaded_tab_title))

    driver = self.NewWebDriver()
    wait = WebDriverWait(driver, timeout=60)

    def scenario(driver, wait):
      # Click the "attach" button to attach a large DOM tree (with event
      # listeners) to the document, wait half a second, click "detach" to detach
      # the DOM tree from the document, wait half a second.
      self._ClickElementByXpath(driver, wait, 'id("attach")')
      time.sleep(0.5)
      self._ClickElementByXpath(driver, wait, 'id("detach")')
      time.sleep(0.5)

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, lambda: scenario(driver, wait))


class ChromeEndureGmailTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Gmail."""

  _webapp_name = 'Gmail'
  _tab_title_substring = 'Gmail'

  def setUp(self):
    ChromeEndureBaseTest.setUp(self)

    # Log into a test Google account and open up Gmail.
    self._LoginToGoogleAccount(account_key='test_google_account_gmail')
    self.NavigateToURL('http://www.gmail.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(self._tab_title_substring in loaded_tab_title,
                    msg='Loaded tab title does not contain "%s": "%s"' %
                        (self._tab_title_substring, loaded_tab_title))

    self._driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    self._wait = WebDriverWait(self._driver, timeout=60)

    # Wait until Gmail's 'canvas_frame' loads and the 'Inbox' link is present.
    # TODO(dennisjeffrey): Check with the Gmail team to see if there's a better
    # way to tell when the webpage is ready for user interaction.
    self._wait.until(
        self._SwitchToCanvasFrame)  # Raises exception if the timeout is hit.
    # Wait for the inbox to appear.
    self._wait.until(lambda _: self._GetElement(
                         self._driver.find_element_by_partial_link_text,
                         'Inbox'))

  def _SwitchToCanvasFrame(self, driver):
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

  def _GetLatencyDomElement(self):
    """Returns a reference to the latency info element in the Gmail DOM."""
    return self._wait.until(
        lambda _: self._GetElement(
            self._driver.find_element_by_xpath,
            '//span[starts-with(text(), "Why was the last action slow?")]'))

  def _WaitUntilDomElementRemoved(self, dom_element):
    """Waits until the given element is no longer attached to the DOM.

    Args:
      dom_element: A selenium.webdriver.remote.WebElement object.
    """
    def _IsElementStale():
      try:
        dom_element.tag_name
      except selenium.common.exceptions.StaleElementReferenceException:
        return True
      return False

    self.WaitUntil(_IsElementStale, timeout=60, expect_retval=True)

  def _ClickElementAndRecordLatency(self, element, test_description,
                                    action_description):
    """Clicks a DOM element and records the latency associated with that action.

    Args:
      element: A selenium.webdriver.remote.WebElement object to click.
      test_description: A string description of what the test does, used for
          outputting results to be graphed.  Should not contain spaces.  For
          example, 'ComposeDiscard' for Gmail.
      action_description: A string description of what action is being
          performed.  Should not contain spaces.  For example, 'Compose'.
    """
    latency_dom_element = self._GetLatencyDomElement()
    element.click()
    # Wait for the old latency value to be removed, before getting the new one.
    self._WaitUntilDomElementRemoved(latency_dom_element)

    latency_dom_element = self._GetLatencyDomElement()
    match = re.search(r'\[(\d+) ms\]', latency_dom_element.text)
    if match:
      latency = int(match.group(1))
      result = [(self._iteration_num, latency)]
      self._OutputPerfGraphValue(
          '%sLatency' % action_description, result, 'msec',
          graph_name='%s%s-%sLatency' % (self._webapp_name, test_description,
                                         action_description),
          units_x='iteration', standalone_graphing_only=True)
    else:
      logging.warning('Could not identify latency value.')

  def testGmailComposeDiscard(self):
    """Continuously composes/discards an e-mail before sending.

    This test continually composes/discards an e-mail using Gmail, and
    periodically gathers performance stats that may reveal memory bloat.
    """
    test_description = 'ComposeDiscard'

    def scenario():
      # Click the "Compose" button, enter some text into the "To" field, enter
      # some text into the "Subject" field, then click the "Discard" button to
      # discard the message.
      compose_button = self._wait.until(lambda _: self._GetElement(
                                            self._driver.find_element_by_xpath,
                                            '//div[text()="COMPOSE"]'))
      self._ClickElementAndRecordLatency(
          compose_button, test_description, 'Compose')

      to_field = self._wait.until(lambda _: self._GetElement(
                                      self._driver.find_element_by_name, 'to'))
      to_field.send_keys('nobody@nowhere.com')

      subject_field = self._wait.until(lambda _: self._GetElement(
                                           self._driver.find_element_by_name,
                                           'subject'))
      subject_field.send_keys('This message is about to be discarded')

      discard_button = self._wait.until(lambda _: self._GetElement(
                                            self._driver.find_element_by_xpath,
                                            '//div[text()="Discard"]'))
      discard_button.click()

      # Wait for the message to be discarded, assumed to be true after the
      # "To" field is removed from the webpage DOM.
      self._wait.until(lambda _: not self._GetElement(
                           self._driver.find_element_by_name, 'to'))

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, scenario)

  # TODO(dennisjeffrey): Remove this test once the Gmail team is done analyzing
  # the results after the test runs for a period of time.
  def testGmailComposeDiscardSleep(self):
    """Like testGmailComposeDiscard, but sleeps for 30s between iterations.

    This is a temporary test requested by the Gmail team to compare against the
    results from testGmailComposeDiscard above.
    """
    test_description = 'ComposeDiscardSleep'

    def scenario():
      # Click the "Compose" button, enter some text into the "To" field, enter
      # some text into the "Subject" field, then click the "Discard" button to
      # discard the message.  Finally, sleep for 30 seconds.
      compose_button = self._wait.until(lambda _: self._GetElement(
                                            self._driver.find_element_by_xpath,
                                            '//div[text()="COMPOSE"]'))
      self._ClickElementAndRecordLatency(
          compose_button, test_description, 'Compose')

      to_field = self._wait.until(lambda _: self._GetElement(
                                      self._driver.find_element_by_name, 'to'))
      to_field.send_keys('nobody@nowhere.com')

      subject_field = self._wait.until(lambda _: self._GetElement(
                                           self._driver.find_element_by_name,
                                           'subject'))
      subject_field.send_keys('This message is about to be discarded')

      discard_button = self._wait.until(lambda _: self._GetElement(
                                            self._driver.find_element_by_xpath,
                                            '//div[text()="Discard"]'))
      discard_button.click()

      # Wait for the message to be discarded, assumed to be true after the
      # "To" field is removed from the webpage DOM.
      self._wait.until(lambda _: not self._GetElement(
                           self._driver.find_element_by_name, 'to'))

      logging.debug('Sleeping 30 seconds.')
      time.sleep(30)

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, scenario)

  def testGmailAlternateThreadlistConversation(self):
    """Alternates between threadlist view and conversation view.

    This test continually clicks between the threadlist (Inbox) and the
    conversation view (e-mail message view), and periodically gathers
    performance stats that may reveal memory bloat.
    """
    test_description = 'ThreadConversation'

    def scenario():
      # Click an e-mail to see the conversation view, wait 1 second, click the
      # "Inbox" link to see the threadlist, wait 1 second.

      # Find the first thread (e-mail) identified by a "span" tag that contains
      # an "email" attribute.  Then click it and wait for the conversation view
      # to appear (assumed to be visible when a particular div exists on the
      # page).
      thread = self._wait.until(
          lambda _: self._GetElement(self._driver.find_element_by_xpath,
                                     '//span[@email]'))
      self._ClickElementAndRecordLatency(
          thread, test_description, 'Conversation')
      self._WaitForElementByXpath(
          self._driver, self._wait, '//div[text()="Click here to "]')
      time.sleep(1)

      # Find the inbox link and click it.  Then wait for the inbox to be shown
      # (assumed to be true when the particular div from the conversation view
      # no longer appears on the page).
      inbox = self._wait.until(
          lambda _: self._GetElement(self._driver.find_element_by_xpath,
                                     '//a[starts-with(text(), "Inbox")]'))
      self._ClickElementAndRecordLatency(inbox, test_description, 'Threadlist')
      self._wait.until(
          lambda _: not self._GetElement(
              self._driver.find_element_by_xpath,
              '//div[text()="Click here to "]'))
      time.sleep(1)

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, scenario)

  def testGmailAlternateTwoLabels(self):
    """Continuously alternates between two labels.

    This test continually clicks between the "Inbox" and "Sent Mail" labels,
    and periodically gathers performance stats that may reveal memory bloat.
    """
    test_description = 'AlternateLabels'

    def scenario():
      # Click the "Sent Mail" label, wait for 1 second, click the "Inbox" label,
      # wait for 1 second.

      # Click the "Sent Mail" label, then wait for the tab title to be updated
      # with the substring "sent".
      sent = self._wait.until(
          lambda _: self._GetElement(self._driver.find_element_by_xpath,
                                     '//a[starts-with(text(), "Sent Mail")]'))
      self._ClickElementAndRecordLatency(sent, test_description, 'SentMail')
      self.assertTrue(
          self.WaitUntil(lambda: 'Sent Mail' in self.GetActiveTabTitle(),
                         timeout=60, expect_retval=True, retry_sleep=1),
          msg='Timed out waiting for Sent Mail to appear.')
      time.sleep(1)

      # Click the "Inbox" label, then wait for the tab title to be updated with
      # the substring "inbox".
      inbox = self._wait.until(
          lambda _: self._GetElement(self._driver.find_element_by_xpath,
                                     '//a[starts-with(text(), "Inbox")]'))
      self._ClickElementAndRecordLatency(inbox, test_description, 'Inbox')
      self.assertTrue(
          self.WaitUntil(lambda: 'Inbox' in self.GetActiveTabTitle(),
                         timeout=60, expect_retval=True, retry_sleep=1),
          msg='Timed out waiting for Inbox to appear.')
      time.sleep(1)

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, scenario)

  def testGmailExpandCollapseConversation(self):
    """Continuously expands/collapses all messages in a conversation.

    This test opens up a conversation (e-mail thread) with several messages,
    then continually alternates between the "Expand all" and "Collapse all"
    views, while periodically gathering performance stats that may reveal memory
    bloat.
    """
    test_description = 'ExpandCollapse'

    # Enter conversation view for a particular thread.
    thread = self._wait.until(
        lambda _: self._GetElement(self._driver.find_element_by_xpath,
                                   '//span[@email]'))
    thread.click()
    self._WaitForElementByXpath(
        self._driver, self._wait, '//div[text()="Click here to "]')

    def scenario():
      # Click on the "Expand all" icon, wait for 1 second, click on the
      # "Collapse all" icon, wait for 1 second.

      # Click the "Expand all" icon, then wait for that icon to be removed from
      # the page.
      expand = self._wait.until(
          lambda _: self._GetElement(self._driver.find_element_by_xpath,
                                     '//img[@alt="Expand all"]'))
      self._ClickElementAndRecordLatency(expand, test_description, 'ExpandAll')
      self._wait.until(
          lambda _: self._GetElement(
              self._driver.find_element_by_xpath,
              '//img[@alt="Expand all"]/parent::*/parent::*/parent::*'
              '[@style="display: none; "]'))
      time.sleep(1)

      # Click the "Collapse all" icon, then wait for that icon to be removed
      # from the page.
      collapse = self._wait.until(
          lambda _: self._GetElement(self._driver.find_element_by_xpath,
                                     '//img[@alt="Collapse all"]'))
      collapse.click()
      self._wait.until(
          lambda _: self._GetElement(
              self._driver.find_element_by_xpath,
              '//img[@alt="Collapse all"]/parent::*/parent::*/parent::*'
              '[@style="display: none; "]'))
      time.sleep(1)

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, scenario)


class ChromeEndureDocsTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Google Docs."""

  _webapp_name = 'Docs'
  _tab_title_substring = 'Docs'

  def setUp(self):
    ChromeEndureBaseTest.setUp(self)

    # Log into a test Google account and open up Google Docs.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://docs.google.com')
    self.assertTrue(
        self.WaitUntil(lambda: self._tab_title_substring in
                               self.GetActiveTabTitle(),
                       timeout=60, expect_retval=True, retry_sleep=1),
        msg='Timed out waiting for Docs to load. Tab title is: %s' %
            self.GetActiveTabTitle())

    self._driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    self._wait = WebDriverWait(self._driver, timeout=60)

  def testDocsAlternatelyClickLists(self):
    """Alternates between two different document lists.

    This test alternately clicks the "Owned by me" and "Home" buttons in
    Google Docs, and periodically gathers performance stats that may reveal
    memory bloat.
    """
    test_description = 'AlternateLists'

    def scenario():
      # Click the "Owned by me" button, wait for 1 second, click the "Home"
      # button, wait for 1 second.

      # Click the "Owned by me" button and wait for a resulting div to appear.
      if not self._ClickElementByXpath(
          self._driver, self._wait, '//div[text()="Owned by me"]'):
        self._num_errors += 1
      if not self._WaitForElementByXpath(
          self._driver, self._wait,
          '//div[@title="Owned by me filter.  Use backspace or delete to '
          'remove"]'):
        self._num_errors += 1
      time.sleep(1)

      # Click the "Home" button and wait for a resulting div to appear.
      if not self._ClickElementByXpath(
          self._driver, self._wait, '//div[text()="Home"]'):
        self._num_errors += 1
      if not self._WaitForElementByXpath(
          self._driver, self._wait,
          '//div[@title="Home filter.  Use backspace or delete to remove"]'):
        self._num_errors += 1
      time.sleep(1)

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, scenario)


class ChromeEndurePlusTest(ChromeEndureBaseTest):
  """Long-running performance tests for Chrome using Google Plus."""

  _webapp_name = 'Plus'
  _tab_title_substring = 'Google+'

  def setUp(self):
    ChromeEndureBaseTest.setUp(self)

    # Log into a test Google account and open up Google Plus.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://plus.google.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(self._tab_title_substring in loaded_tab_title,
                    msg='Loaded tab title does not contain "%s": "%s"' %
                        (self._tab_title_substring, loaded_tab_title))

    self._driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    self._wait = WebDriverWait(self._driver, timeout=60)

  def testPlusAlternatelyClickStreams(self):
    """Alternates between two different streams.

    This test alternately clicks the "Friends" and "Acquaintances" buttons using
    Google Plus, and periodically gathers performance stats that may reveal
    memory bloat.
    """
    test_description = 'AlternateStreams'

    def scenario():
      # Click the "Friends" button, wait for 1 second, click the "Acquaintances"
      # button, wait for 1 second.

      # Click the "Friends" button and wait for a resulting div to appear.
      if not self._ClickElementByXpath(
          self._driver, self._wait,
          '//a[text()="Friends" and starts-with(@href, "stream/circles")]'):
        self._num_errors += 1
      if not self._WaitForElementByXpath(
          self._driver, self._wait, '//div[text()="Friends"]'):
        self._num_errors += 1
      time.sleep(1)

      # Click the "Acquaintances" button and wait for a resulting div to appear.
      if not self._ClickElementByXpath(
          self._driver, self._wait,
          '//a[text()="Acquaintances" and '
          'starts-with(@href, "stream/circles")]'):
        self._num_errors += 1
      if not self._WaitForElementByXpath(
          self._driver, self._wait, '//div[text()="Acquaintances"]'):
        self._num_errors += 1
      time.sleep(1)

    self._RunControlTest(self._webapp_name, self._tab_title_substring,
                         test_description, scenario)


if __name__ == '__main__':
  pyauto_functional.Main()
