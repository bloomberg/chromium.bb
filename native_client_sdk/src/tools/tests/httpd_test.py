#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import Queue
import sys
import subprocess
import threading
import unittest
import urllib2

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(PARENT_DIR)

import httpd


class HTTPDTest(unittest.TestCase):
  def setUp(self):
    self.server = httpd.LocalHTTPServer('.', 0)

  def tearDown(self):
    self.server.Shutdown()

  def testQuit(self):
    urllib2.urlopen(self.server.GetURL('?quit=1'))
    self.server.process.join(10)  # Wait 10 seconds for the process to finish.
    self.assertFalse(self.server.process.is_alive())


class RunTest(unittest.TestCase):
  def setUp(self):
    self.process = None

  def tearDown(self):
    if self.process and self.process.returncode is None:
      self.process.kill()

  @staticmethod
  def _SubprocessThread(process, queue):
    stdout, stderr = process.communicate()
    queue.put((process.returncode, stdout, stderr))

  def _Run(self, args=None, timeout=None):
    args = args or []
    run_py = os.path.join(PARENT_DIR, 'run.py')
    cmd = [sys.executable, run_py]
    cmd.extend(args)
    self.process = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
    queue = Queue.Queue()
    thread = threading.Thread(target=RunTest._SubprocessThread,
                              args=(self.process, queue))
    thread.daemon = True
    thread.start()
    thread.join(timeout)
    if not thread.is_alive():
      returncode, stdout, stderr = queue.get(False)
      return returncode, stdout, stderr

    return -1, None, None

  def _GetChromeMockArgs(self, page, http_request_type, sleep,
                         expect_to_be_killed=True):
    args = []
    if page:
      args.extend(['-P', page])
    args.append('--')
    args.extend([sys.executable, os.path.join(SCRIPT_DIR, 'chrome_mock.py')])
    if http_request_type:
      args.append('--' + http_request_type)
    if sleep:
      args.extend(['--sleep', str(sleep)])
    if expect_to_be_killed:
      args.append('--expect-to-be-killed')
    return args

  def testQuit(self):
    args = self._GetChromeMockArgs('?quit=1', 'get', sleep=10)
    _, stdout, _ = self._Run(args, timeout=20)
    self.assertTrue('Starting' in stdout)
    self.assertTrue('Expected to be killed' not in stdout)

  def testSubprocessDies(self):
    args = self._GetChromeMockArgs(page=None, http_request_type=None, sleep=0,
                                   expect_to_be_killed=False)
    returncode, stdout, _ = self._Run(args, timeout=10)
    self.assertNotEqual(-1, returncode)
    self.assertTrue('Starting' in stdout)


if __name__ == '__main__':
  unittest.main()
