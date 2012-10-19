#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import devtools_test_base
import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils


class DevToolsNativeMemorySnapshotTest(devtools_test_base.DevToolsTestBase):
  """Test for tracking unknown share in the DevTools native memory snapshots.

  This test navigates the browser to a test page, then takes native memory
  snapshot over remote debugging protocol and prints render process private
  memory size and unknown size extracted from the snapshot. It is used to
  track size of the memory that is not counted by DevTools memory
  instrumentation.

  The test uses Web Page Replay server as a proxy that allows to replay
  the same state of the test pages and avoid heavy network traffic on the
  real web sites. See webpagereplay.ReplayServer documentation to learn how
  to record new page archives.
  """
  def testNytimes(self):
    self.RunTestWithUrl('http://www.nytimes.com/')

  def testCnn(self):
    self.RunTestWithUrl('http://www.cnn.com/')

  def testGoogle(self):
    self.RunTestWithUrl('http://www.google.com/')

  def PrintTestResult(self, hostname, snapshot):
    total = snapshot.GetProcessPrivateMemorySize()
    unknown = snapshot.GetUnknownSize()
    logging.info('Got data for: %s, total size = %d, unknown size = %d' %
        (hostname, total, unknown))

    graph_name = 'Native Memory Unknown Bytes'
    pyauto_utils.PrintPerfResult(graph_name, hostname + ': total', total,
        'bytes')
    pyauto_utils.PrintPerfResult(graph_name, hostname + ': unknown', unknown,
        'bytes')

    if total == 0:
      logging.info('No total process memory size')
      return
    unknown_percent = (float(unknown) / total) * 100
    unknown_percent_string = '%.1f' % unknown_percent
    pyauto_utils.PrintPerfResult('Native Memory Unknown %', hostname,
        unknown_percent_string, '%')


if __name__ == '__main__':
  pyauto_functional.Main()
