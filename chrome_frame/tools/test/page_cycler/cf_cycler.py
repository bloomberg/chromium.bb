#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Automates IE to visit a list of web sites while running CF in full tab mode.

The page cycler automates IE and navigates it to a series of URLs. It is
designed to be run with Chrome Frame configured to load every URL inside
CF full tab mode.

TODO(robertshield): Make use of the python unittest module as per
review comments.
"""

import optparse
import sys
import time
import win32com.client
import win32gui

def LoadSiteList(path):
  """Loads a list of URLs from |path|.

  Expects the URLs to be separated by newlines, with no leading or trailing
  whitespace.

  Args:
    path: The path to a file containing a list of new-line separated URLs.

  Returns:
    A list of strings, each one a URL.
  """
  f = open(path)
  urls = f.readlines()
  f.close()
  return urls

def LaunchIE():
  """Starts up IE, makes it visible and returns the automation object.

  Returns:
    The IE automation object.
  """
  ie = win32com.client.Dispatch("InternetExplorer.Application")
  ie.visible = 1
  win32gui.SetForegroundWindow(ie.HWND)
  return ie

def RunTest(url, ie):
  """Loads |url| into the InternetExplorer.Application instance in |ie|.

   Waits for the Document object to be created and then waits for
   the document ready state to reach READYSTATE_COMPLETE.
  Args:
    url: A string containing the url to navigate to.
    ie: The IE automation object to navigate.
  """

  print "Navigating to " + url
  ie.Navigate(url)
  timer = 0

  READYSTATE_COMPLETE = 4

  last_ready_state = -1
  for retry in xrange(60):
    try:
      # TODO(robertshield): Become an event sink instead of polling for
      # changes to the ready state.
      last_ready_state = ie.Document.ReadyState
      if last_ready_state == READYSTATE_COMPLETE:
        break
    except:
      # TODO(robertshield): Find the precise exception related to ie.Document
      # being not accessible and handle it here.
      print "Unexpected error:", sys.exc_info()[0]
      raise
    time.sleep(1)

  if last_ready_state != READYSTATE_COMPLETE:
    print "Timeout waiting for " + url

def main():
  parser = optparse.OptionParser()
  parser.add_option('-u', '--url_list', default='urllist',
                    help='The path to the list of URLs')
  (opts, args) = parser.parse_args()

  urls = LoadSiteList(opts.url_list)
  ie = LaunchIE()
  for url in urls:
    RunTest(url, ie)
    time.sleep(1)
  ie.visible = 0
  ie.Quit()


if __name__ == '__main__':
  main()
