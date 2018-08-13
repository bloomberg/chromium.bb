#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the client_replay.py script.

To run tests, just call this script with the intended chrome, chromedriver
binaries and an optional test filter. This file interfaces with the
client_replay mainly using the CommandSequence class.
"""
# pylint: disable=g-import-not-at-top, g-bad-import-order
import json
import optparse
import os
import re
import sys
import unittest
import client_replay

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
_PARENT_DIR = os.path.join(_THIS_DIR, os.pardir)
_TEST_DIR = os.path.join(_PARENT_DIR, "test")

sys.path.insert(1, _PARENT_DIR)
import chrome_paths
import util
sys.path.remove(_PARENT_DIR)

sys.path.insert(1, _TEST_DIR)
import unittest_util
import webserver
sys.path.remove(_TEST_DIR)
# pylint: enable=g-import-not-at-top, g-bad-import-order


def SubstituteVariableEntries(s):
  """Identifies and removes items that can legitimately vary between runs."""
  white_list = r'(("(id|userDataDir|frameId|version|ELEMENT|message|timestamp' \
               r'|expiry|chromedriverVersion)": ' \
               r'("[0-9]\.[0-9]*(\.[0-9]*)? \([a-f0-9]*\)"|[^\s},]*))' \
               r'|CDwindow-[A-F0-9]*|cd_frame_id_="[a-f0-9]*")'

  return re.sub(white_list, "<variable_item>", s)


def ClearPort(s):
  """Removes port numbers from urls in the given string."""
  s = re.sub(r":([0-9]){5}/", "<port>", s)
  return re.sub(r"localhost:([0-9]){5}", "localhost:<port>", s)


class ChromeDriverClientReplayTest(unittest.TestCase):
  """Base class for test cases."""

  def __init__(self, *args, **kwargs):
    super(ChromeDriverClientReplayTest, self).__init__(*args, **kwargs)

  @classmethod
  def setUpClass(cls):
    """Starts the server for the necessary pages for testing."""
    cls.http_server = webserver.WebServer(chrome_paths.GetTestData())
    cls.server_url = cls.http_server.GetUrl()

  @classmethod
  def tearDownClass(cls):
    """Tears down the server."""
    cls.http_server.Shutdown()

  def CheckResponsesMatch(self, real, logged):
    """Asserts that the given pair of responses match.

    These are usually the replay response and the logged response.
    Checks that they match, up to differences in session, window, element
    IDs, timestamps, etc.

    Args:
      real: actual response from running the command
      logged: logged response, taken from the log file
    """
    if not real and not logged:
      return

    if isinstance(real, dict) and "message" in real:
      real = "ERROR " + real["message"].split("\n")[0]

    # pylint: disable=unidiomatic-typecheck
    self.assertTrue(type(logged) == type(real)
                    or (isinstance(real, basestring)
                        and isinstance(logged, basestring)))
    # pylint: enable=unidiomatic-typecheck

    if isinstance(real, basestring) \
        and (real[:14] == "<!DOCTYPE html" or real[:5] == "<html"):
      real = "".join(real.split())
      logged = "".join(logged.split())

    if not isinstance(real, basestring):
      real = json.dumps(real)
      logged = json.dumps(logged)

    real = ClearPort(real)
    logged = ClearPort(logged)
    real = SubstituteVariableEntries(real)
    logged = SubstituteVariableEntries(logged)

    self.assertEqual(real, logged)

  def runTest(self, log_file_relative_path):
    """Runs the test.

    Args:
      log_file_relative_path: relative path (from this the directory this file
        is in) to the log file for this test as a string.
    """
    log_file = os.path.join(_THIS_DIR, log_file_relative_path)
    with open(log_file) as lf:
      server = client_replay.StartChromeDriverServer(_CHROMEDRIVER, _OPTIONS)
      chrome_binary = (util.GetAbsolutePathOfUserPath(_OPTIONS.chrome)
                       if _OPTIONS.chrome else None)

      replayer = client_replay.Replayer(lf, server, chrome_binary,
                                        self.server_url)
      real_response = None
      while True:
        command = replayer.command_sequence.NextCommand(real_response)
        logged_response = replayer.command_sequence._last_response
        if not command:
          break
        real_response = replayer.executor.Execute(
            client_replay._COMMANDS[command.name],
            command.GetPayloadPrimitive())

        self.CheckResponsesMatch(real_response["value"],
                                 logged_response.GetPayloadPrimitive())
      server.Kill()

  def testGetPageSource(self):
    self.runTest("test_data/testPageSource.log")

  def testCloseWindow(self):
    self.runTest("test_data/testCloseWindow.log")

  def testConsoleLogSources(self):
    self.runTest("test_data/testConsoleLogSources.log")

  def testIFrameWithExtensionsSource(self):
    self.runTest("test_data/testIFrameWithExtensionsSource.log")

  def testSendingTabKeyMovesToNextInputElement(self):
    self.runTest("test_data/testSendingTabKeyMovesToNextInputElement.log")

  def testUnexpectedalertBehavior(self):
    self.runTest("test_data/testUnexpectedAlertBehavior.log")

  def testDownload(self):
    self.runTest("test_data/testDownload.log")

  def testCanSwitchToPrintPreviewDialog(self):
    self.runTest("test_data/testCanSwitchToPrintPreviewDialog.log")

  def testClearElement(self):
    self.runTest("test_data/testClearElement.log")

  def testEmulateNetwork(self):
    self.runTest("test_data/testEmulateNetwork.log")

  def testSwitchTo(self):
    self.runTest("test_data/testSwitchTo.log")

  def testEvaluateScript(self):
    self.runTest("test_data/testEvaluateScript.log")

  def testEvaluateInvalidScript(self):
    self.runTest("test_data/testEvaluateInvalidScript.log")

  def testGetTitle(self):
    self.runTest("test_data/testGetTitle.log")

  def testSendCommand(self):
    self.runTest("test_data/testSendCommand.log")

  def testSessionHandling(self):
    self.runTest("test_data/testSessionHandling.log")

  def testRunMethod(self):
    """Check the Replayer.run method, which the other tests bypass."""
    log_path = os.path.join(_THIS_DIR, "test_data/testGetTitle.log")
    with open(log_path) as lf:
      server = client_replay.StartChromeDriverServer(_CHROMEDRIVER, _OPTIONS)
      chrome_binary = (util.GetAbsolutePathOfUserPath(_OPTIONS.chrome)
                       if _OPTIONS.chrome else None)

      client_replay.Replayer(lf, server, chrome_binary, self.server_url).Run()
      server.Kill()

  def testChromeBinary(self):
    self.runTest("test_data/testChromeBinary.log")

def main():
  usage = "usage: %prog <chromedriver binary> [options]"
  parser = optparse.OptionParser(usage=usage)
  parser.add_option(
      "", "--output-log-path",
      help="Output verbose server logs to this file")
  parser.add_option(
      "", "--chrome", help="Path to the desired Chrome binary.")
  parser.add_option(
      "", "--filter", type="string", default="*",
      help="Filter for specifying what tests to run, \"*\" will run all. E.g., "
      "*testRunMethod")

  global _OPTIONS, _CHROMEDRIVER
  _OPTIONS, args = parser.parse_args()
  _CHROMEDRIVER = util.GetAbsolutePathOfUserPath(args[0])
  if not os.path.exists(_CHROMEDRIVER):
    parser.error("Path given by --chromedriver is invalid.\n"
                 'Please run "%s --help" for help' % __file__)
  if _OPTIONS.chrome and not os.path.exists(_OPTIONS.chrome):
    parser.error("Path given by --chrome is invalid.\n"
                 'Please run "%s --help" for help' % __file__)

  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      sys.modules[__name__])
  tests = unittest_util.FilterTestSuite(all_tests_suite, _OPTIONS.filter)
  result = unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(tests)
  sys.exit(len(result.failures) + len(result.errors))

if __name__ == "__main__":
  main()