#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import tempfile

import devtools_test_base
import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils


class DevToolsInstrumentedObjectsCheck(devtools_test_base.DevToolsTestBase):
  """Test for checking that all instrumented objects are allocated by tcmalloc.

  This test navigates the browser to a test page, then takes native memory
  snapshot over remote debugging protocol and prints the number of objects
  that were counted by the DevTools memory instrumentation and how many of
  them have not been allocated by tcmalloc. Ideally the latter number should
  be 0.

  The test starts browser with HEAPPROFILE environment variable to enable
  tcmalloc heap profiler which is required by the memory instrumentation to
  check which of the instrumented objects have actually been allocated on the
  heap.

  The test uses Web Page Replay server as a proxy that allows to replay
  the same state of the test pages and avoid heavy network traffic on the
  real web sites. See webpagereplay.ReplayServer documentation to learn how
  to record new page archives.
  """

  def setUp(self):
    # Make sure Chrome is started with tcmalloc heap profiler enabled. Dump
    # profiles into a temporary directory that will be destroyed when the test
    # completes.
    self._tempdir = tempfile.mkdtemp(prefix='devtools-test')
    os.environ['HEAPPROFILE'] = os.path.join(self._tempdir, 'heap-profile.')
    super(DevToolsInstrumentedObjectsCheck, self).setUp()

  def tearDown(self):
    super(DevToolsInstrumentedObjectsCheck, self).tearDown()
    del os.environ['HEAPPROFILE']
    if self._tempdir:
      pyauto_utils.RemovePath(self._tempdir)

  def testNytimes(self):
    self.RunTestWithUrl('http://www.nytimes.com/')

  def testCnn(self):
    self.RunTestWithUrl('http://www.cnn.com/')

  def testGoogle(self):
    self.RunTestWithUrl('http://www.google.com/')

  def PrintTestResult(self, hostname, snapshot):
    total = snapshot.GetProcessPrivateMemorySize()
    counted_objects = snapshot.GetInstrumentedObjectsCount()
    counted_unknown_objects = snapshot.GetNumberOfInstrumentedObjectsNotInHeap()
    if counted_objects is None or counted_unknown_objects is None:
      logging.info('No information about number of instrumented objects.')
      return
    logging.info('Got data for: %s, objects count = %d (unknown = %d) ' %
        (hostname, counted_objects, counted_unknown_objects))
    pyauto_utils.PrintPerfResult('DevTools Unknown Instrumented Objects',
        hostname, counted_unknown_objects, 'objects')


if __name__ == '__main__':
  pyauto_functional.Main()
