# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WPT WebDriver tests runner."""

import pytest
import os
import argparse
import sys
import json
import time
import logging

_log = logging.getLogger(__name__)

_TEST_DIR = os.path.abspath(os.path.dirname(__file__))
_CHROMEDRIVER_DIR = os.path.join(_TEST_DIR, os.pardir)
SRC_DIR = os.path.join(_CHROMEDRIVER_DIR, os.pardir, os.pardir, os.pardir)
_CLIENT_DIR = os.path.join(_CHROMEDRIVER_DIR, "client")
_SERVER_DIR = os.path.join(_CHROMEDRIVER_DIR, "server")

sys.path.insert(0, _CHROMEDRIVER_DIR)
import util

sys.path.insert(0, _SERVER_DIR)
import server

BLINK_TOOLS_PATH = 'third_party/blink/tools'
BLINK_TOOLS_ABS_PATH = os.path.join(SRC_DIR, BLINK_TOOLS_PATH)

sys.path.insert(0, BLINK_TOOLS_ABS_PATH)
from blinkpy.common.host import Host
from blinkpy.common.system.log_utils import configure_logging

WD_CLIENT_PATH = 'blinkpy/third_party/wpt/wpt/tools/webdriver'
WEBDRIVER_CLIENT_ABS_PATH = os.path.join(BLINK_TOOLS_ABS_PATH, WD_CLIENT_PATH)


class WebDriverTestResult(object):
  def __init__(self, test_name, test_status, messsage=None):
    self.test_name = test_name
    self.test_status = test_status
    self.message = messsage

class SubtestResultRecorder(object):
  def __init__(self):
    self.result = []

  def pytest_runtest_logreport(self, report):
    if report.passed and report.when == "call":
      self.record_pass(report)
    elif report.failed:
      if report.when != "call":
        self.record_error(report)
      else:
        self.record_fail(report)
    elif report.skipped:
      self.record_skip(report)

  def record_pass(self, report):
    self.record(report.nodeid, "PASS")

  def record_fail(self, report):
    message = report.longrepr.reprcrash.message
    self.record(report.nodeid, "FAIL", message=message)

  def record_error(self, report):
    # error in setup/teardown
    if report.when != "call":
      message = "error in %s" % report.when
    self.record(report.nodeid, "FAIL", message)

  def record_skip(self, report):
    self.record(report.nodeid, "FAIL",
                "In-test skip decorators are disallowed.")

  def record(self, test, status, message=None):
    self.result.append(WebDriverTestResult(
        test, status, message))

def set_up_config(chromedriver_server):
  # These envs are used to create a WebDriver session in the fixture.py file.
  os.environ["WD_HOST"] = chromedriver_server.GetHost()
  os.environ["WD_PORT"] = str(chromedriver_server.GetPort())
  os.environ["WD_CAPABILITIES"] = json.dumps({
      "goog:chromeOptions": {
          "w3c": True,
          "prefs": {
              "profile": {
                  "default_content_setting_values": {
                      "popups": 1
                  }
              }
          },
          "args": [
              "--host-resolver-rules="
              "MAP nonexistent.*.test ~NOTFOUND, "
              "MAP *.test 127.0.0.1"
          ]

      }
  })

  # Port numbers are defined at
  # https://cs.chromium.org/chromium/src/third_party/blink/tools
  # /blinkpy/web_tests/servers/wptserve.py?l=23&rcl=375b34c6ba64
  # 5d00c1413e4c6106c7bb74581c85
  os.environ["WD_SERVER_CONFIG"] = json.dumps({
    "browser_host": "web-platform.test",
    "ports": {"ws": [9001], "wss": [9444], "http": [8001], "https": [8444]}})

def run_test(path):
  subtests = SubtestResultRecorder()
  pytest.main(["-s", path], plugins=[subtests])
  return subtests.result

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.description = __doc__
  parser.add_argument(
      '--chromedriver',
      required=True,
      help='Path to chromedriver binary')
  parser.add_argument(
      '--log-path',
      help='Output verbose server logs to this file')
  parser.add_argument(
      '--chrome', help='Path to chrome binary')
  parser.add_argument(
      '--isolated-script-test-output',
      help='JSON output file used by swarming')
  parser.add_argument(
      '--isolated-script-test-perf-output',
      help='JSON perf output file used by swarming, ignored')
  parser.add_argument(
      '--test-path',
      help='Path to the WPT WebDriver tests')
  parser.add_argument(
      '-v', '--verbose', action='store_true',
      help='log extra details that may be helpful when debugging')

  options = parser.parse_args()

  log_level = logging.DEBUG if options.verbose else logging.INFO
  configure_logging(logging_level=log_level, include_time=True)

  # Starts WPT Serve to serve the WPT WebDriver test content.
  port = Host().port_factory.get()
  port.start_wptserve()

  options.chromedriver = util.GetAbsolutePathOfUserPath(options.chromedriver)
  if (not os.path.exists(options.chromedriver) and
      util.GetPlatformName() == 'win' and
      not options.chromedriver.lower().endswith('.exe')):
    options.chromedriver = options.chromedriver + '.exe'

  if not os.path.exists(options.chromedriver):
    parser.error('Path given by --chromedriver is invalid.\n' +
                 'Please run "%s --help" for help' % __file__)

  chromedriver_server = server.Server(options.chromedriver, options.log_path)

  if not chromedriver_server.IsRunning():
    _log.error('ChromeDriver is not running.')
    sys.exit(1)

  set_up_config(chromedriver_server)

  test_path = options.test_path
  start_time = time.time()
  test_results = []
  sys.path.insert(0, WEBDRIVER_CLIENT_ABS_PATH)
  try:
    if os.path.isfile(test_path):
      test_results = run_test(test_path)
    elif os.path.isdir(test_path):
      for root, dirnames, filenames in os.walk(test_path):
        for filename in filenames:
          if '__init__' in filename:
            continue
          test_file = os.path.join(root, filename)
          test_results += run_test(test_file)
    else:
      _log.error('%s is not a file nor directory.' % test_path)
      sys.exit(1)
  except KeyboardInterrupt as e:
    _log.error(e)
  finally:
    chromedriver_server.Kill()
    port.stop_wptserve()

  if options.isolated_script_test_output:
    output = {
      'interrupted': False,
      'num_failures_by_type': { },
      'path_delimiter': '.',
      'seconds_since_epoch': start_time,
      'tests': { },
      'version': 3,
    }

    success_count = 0
    for test_result in test_results:
      output['tests'][test_result.test_name] = {
        'expected': 'PASS',
        'actual': test_result.test_status,
        'message': test_result.message
      }

      if test_result.test_status == 'PASS':
        success_count += 1

    output['num_failures_by_type']['PASS'] = success_count
    output['num_failures_by_type']['FAIL'] = len(test_results) - success_count

    with open(options.isolated_script_test_output, 'w') as fp:
      json.dump(output, fp)

  sys.exit(0)
