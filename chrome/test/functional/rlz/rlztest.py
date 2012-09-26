#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import chromium_proxy_server_ex
import commands
import logging
import optparse
import os
import re
import selenium.selenium
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import _winreg
import selenium.webdriver.common.keys

from selenium import webdriver
from selenium.webdriver.common.keys import Keys as Keys
from selenium.webdriver.support.ui import WebDriverWait

class RlzTest(unittest.TestCase):

  proxy_server_file = ''
  chrome_driver_path = ''

  def setUp(self):
    """Performs necessary setup work before running each test in this class."""
    # Delete RLZ key Folder
    self.deleteRegistryKey()
    # Launch Proxy Server
    print ('Serving clients: 127.0.0.1')
    self.proxy_server = subprocess.Popen([
        'python',
        RlzTest.proxy_server_file,
        '--client=127.0.0.1',
        '--port=8080',
        '--urls=http://clients1.google.com/tools/pso/ping,www.google.com'])
    print('\nLaunching Chrome...')
    # Launch chrome and set proxy.
    self.temp_data_dir = tempfile.mkdtemp()
    self.launchChrome(self.temp_data_dir)

  def tearDown(self):
    """Kills the chrome driver after after the test method has been called."""
    # Terminate the chrome driver.
    print '\nTerminating Chrome Driver...'
    self.driver.quit()

    # Kill proxy server.
    print '\nKilling Proxy Server...'
    subprocess.Popen.kill(self.proxy_server)

    # Delete temp profile directory
    try:
        shutil.rmtree(self.temp_data_dir) # delete directory
    except OSError, e:
        if e.errno != 2: # code 2 - no such file or directory
          raise

  def launchChrome(self, data_directory):
    """Launch chrome using chrome driver.

    Args:
      data_directory: Temp directory to store preference data.
    """
    service = webdriver.chrome.service.Service(RlzTest.chrome_driver_path)
    service.start()
    self.driver = webdriver.Remote(
        service.service_url, {
        'proxy': {'proxyType': 'manual', 'httpProxy': 'localhost:8080'},
        'chrome.nativeEvents': True,
        'chrome.switches': ['disable-extensions',
                            r'user-data-dir=' + data_directory]})

  def deleteRegistryKey(self):
    """Delete RLZ key Folder from win registry."""
    try:
      hkey = _winreg.OpenKey(_winreg.HKEY_CURRENT_USER,
                             'Software\\Google\\Common\\Rlz')
    except _winreg.error, err:
      return True
    if(hkey):
        _winreg.DeleteKey(_winreg.HKEY_CURRENT_USER,
                          'Software\\Google\\Common\\Rlz\\Events\\C')
        _winreg.DeleteKey(_winreg.HKEY_CURRENT_USER,
                          'Software\\Google\\Common\\Rlz\\StatefulEvents\\C')
        _winreg.DeleteKey(_winreg.HKEY_CURRENT_USER,
                          'Software\\Google\\Common\\Rlz\\Events')
        _winreg.DeleteKey(_winreg.HKEY_CURRENT_USER,
                          'Software\\Google\\Common\\Rlz\\StatefulEvents')
        _winreg.DeleteKey(_winreg.HKEY_CURRENT_USER,
                          'Software\\Google\\Common\\Rlz\\PTimes')
        _winreg.DeleteKey(_winreg.HKEY_CURRENT_USER,
                          'Software\\Google\\Common\\Rlz\\RLZs')
        _winreg.DeleteKey(_winreg.HKEY_CURRENT_USER,
                          'Software\\Google\\Common\\Rlz')

  def GetKeyValueNames(self, key, subkey):
    """Get the values for particular subkey

    Args:
      key: Key is one of the predefined HKEY_* constants.
      subkey: It is a string that identifies the sub_key to delete.
    """
    list_names = []
    counter = 0
    try:
      hkey = _winreg.OpenKey(key, subkey)
    except _winreg.error, err:
      return -1
    while True:
      try:
        value = _winreg.EnumValue(hkey, counter)
        list_names.append(value[0])
        counter += 1
      except _winreg.error:
        break
    hkey.Close()
    return list_names

  def _AssertEventsInPing(self, log_file, excepted_event_list, readFile=1):
    """ Asserts events in ping appended.

    Args:
      contents: String variable contains contents of log file.
      excepted_event_list: List of expected events in ping.
      readFile: Reading order for file. Default is 1 (Top to Bottom).
    """
    for line in log_file[::readFile]:
      if(re.search('events=', line)):
        event_start = line.find('events=')
        event_end = line.find('&rep', event_start)
        events = line[event_start + 7 : event_end]
        events_List = events.split(',')
        print 'event_list',events_List
        break
    # Validate events in url ping.
    for event in excepted_event_list:
      self.assertTrue(event in events_List)
    # Validate brand code in url ping.
    self.assertTrue(re.search('CHMZ', line))
    # Print first chrome launch ping on Console.
    start = line.find('http://clients1.google.com/tools/'+
                      'pso/ping?as=chrome&brand=CHMZ')
    end = line.find('http://www', start)
    print '\nChrome Launch ping sent :\n', line[start:end]


  def _AssertEventsInRegistry(self, excepted_reg_keys):
    """ Asserts events reported in win registry.

    Args:
      excepted_reg_keys: List of expected events in win registry.
    """
    list_key=self.GetKeyValueNames(_winreg.HKEY_CURRENT_USER,
               'Software\Google\Common\Rlz\StatefulEvents\C')
    print ('\nList of event reported to registry-'
           'Software\Google\Common\Rlz\StatefulEvents:', list_key)
    for key in excepted_reg_keys:
      self.assertTrue(key in list_key)

  def _AssertRlzValues(self, log_file, readFile=1):
    """ Asserts RLZ values.

    Args:
      log_file: String variable contains contents of log file.
      readFile: Reading order for file. Default is 1 (Top to Bottom).
    """
    for line in log_file[::readFile]:
      if(re.search('events=', line)):
        event_start = line.find('rlz=')
        event_end = line.find('&id', event_start)
        events = line[event_start + 4 : event_end]
        events_List = events.split(',')
        self.assertTrue('C1:' in events_List)
        self.assertTrue('C2:' in events_List)

  def _searchFromOmnibox(self, searchString):
    """ Asserts RLZ values.

    Args:
      searchString: Input string to be searched.
    """
    self.driver.switch_to_active_element().send_keys(Keys.CONTROL + 'l')
    self.driver.switch_to_active_element().send_keys(searchString)
    self.driver.switch_to_active_element().send_keys(Keys.ENTER)
    time.sleep(2)

  def testRlzPingAtFirstChromeLaunch(self):
    """Test rlz ping when chrome is launched for first time."""
    # Wait for 100 sec till chrome sends ping to server.
    time.sleep(100)
    self.driver.get('http://www.google.com')

    # Open log file.
    log_file = open(
        os.getcwd() + '\chromium_proxy_server.log', 'r', 1).readlines()

    # Validate events first chrome launch(C1I,C2I,C1S) are appended in ping.
    excepted_events = ['C1S', 'C1I', 'C2I']
    self._AssertEventsInPing(log_file, excepted_events)

    # Validate events in win registry.
    excepted_reg_keys = ['C1I', 'C2I']
    self._AssertEventsInRegistry(excepted_reg_keys)

  def testRlzPingForFirstSearch(self):
    """Test rlz ping when first search is performed in chrome."""
    # Type search string in omnibox.
    self._searchFromOmnibox('java')
    print '\nCurrent Url before chrome ping sent:\n', self.driver.current_url

    # Assert brand code 'CHMZ' is not appended in search string.
    self.assertFalse(re.search('CHMZ', self.driver.current_url))

    # Wait for 100 sec till chrome sends ping to server.
    time.sleep(100)

    # Open log file.
    log_file = open(
        os.getcwd() + '\chromium_proxy_server.log', 'r', 1).readlines()

    # Validate events first chrome launch(C1I,C2I,C1S) and
    # first search(C1F) are appended in ping.
    excepted_events = ['C1S', 'C1I', 'C2I', 'C1F']
    self._AssertEventsInPing(log_file, excepted_events)

    # Assert C1, C2 rlz value appended in ping
    self._AssertRlzValues(log_file)

    # Type search string in omnibox after ping is sent to server.
    self._searchFromOmnibox('java')
    print '\nCurrent Url after chrome ping sent:\n', self.driver.current_url

    # Assert brand code 'CHMZ' is appended in search string.
    self.assertTrue(re.search('CHMZ', self.driver.current_url))

    # Validate events in win registry.
    excepted_reg_keys=['C1I', 'C2I', 'C1F']
    self._AssertEventsInRegistry(excepted_reg_keys)

    # Assert the log for search ping with query string/brand code appended.
    log_file = open(
        os.getcwd() + '\chromium_proxy_server.log', 'r', 1).readlines()
    searchStringFound = False
    for line in log_file[::-1]:
      if(re.search('search?', line)):
        self.assertTrue(re.search('java', line))
        self.assertTrue(re.search('CHMZ', line))
        print '\nChrome search ping send\n', line
        searchStringFound = True
        break
    self.assertTrue(searchStringFound, 'Search Query String Not Found')

def rlzInput():
    proxy_server_file = raw_input("Enter Proxy Server File Name: ")
    chrome_driver_path = raw_input("Enter chrome driver path in"+
                                   "your system(c:\\chrome\\..):")
    return (proxy_server_file, chrome_driver_path)

if __name__ == '__main__':
  server, driver = rlzInput()
  RlzTest.proxy_server_file = server
  RlzTest.chrome_driver_path = driver
  unittest.main()
