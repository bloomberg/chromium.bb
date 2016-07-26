# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chromite.lib.metrics."""

from __future__ import print_function

import itertools
import mock
import multiprocessing
import Queue

from chromite.lib import ts_mon_config
from chromite.lib import cros_test_lib


# pylint: disable=protected-access


class TestConsumeMessages(cros_test_lib.MockTestCase):
  """Test that ConsumeMessages works correctly."""

  def setUp(self):
    # Every call to "time.time()" will look like 1 second has passed.
    # Nb. we only want to mock out ts_mon_config's view of time, otherwise
    # things like Process.join(10) won't sleep.
    time_mock = self.PatchObject(ts_mon_config, 'time')
    time_mock.time.side_effect = itertools.count(0)
    self.PatchObject(ts_mon_config.metrics, 'flush')
    self.PatchObject(ts_mon_config, 'SetupTsMonGlobalState')


  def testNoneEndsProcess(self):
    """Putting None on the Queue should immediately end the consumption loop."""
    q = Queue.Queue()
    q.put(None)

    ts_mon_config._ConsumeMessages(q, [''], {})

    ts_mon_config.SetupTsMonGlobalState.assert_called_once_with(
        '', auto_flush=False)
    ts_mon_config.time.time.assert_not_called()
    ts_mon_config.metrics.flush.assert_not_called()

  def testConsumeOneMetric(self):
    """Tests that sending one metric calls flush once."""
    q = Queue.Queue()
    mock_metric = mock.Mock()
    q.put((mock_metric, 'mock_name', ['arg1'], {'kwarg1': 'value'}))
    q.put(None)

    ts_mon_config._ConsumeMessages(q, [''], {})

    self.assertEqual(2, ts_mon_config.time.time.call_count)
    ts_mon_config.time.sleep.assert_called_once_with(
        ts_mon_config.FLUSH_INTERVAL - 1)
    ts_mon_config.metrics.flush.assert_called_once_with()
    mock_metric.mock_name.assert_called_once_with('arg1', kwarg1='value')

  def testConsumeTwoMetrics(self):
    """Tests that sending two metrics only calls flush once."""
    q = Queue.Queue()
    mock_metric = mock.Mock()
    q.put((mock_metric, 'mock_name1', ['arg1'], {'kwarg1': 'value'}))
    q.put((mock_metric, 'mock_name2', ['arg2'], {'kwarg2': 'value'}))
    q.put(None)

    ts_mon_config._ConsumeMessages(q, [''], {})

    self.assertEqual(3, ts_mon_config.time.time.call_count)
    ts_mon_config.time.sleep.assert_called_once_with(
        ts_mon_config.FLUSH_INTERVAL - 2)
    ts_mon_config.metrics.flush.assert_called_once_with()
    mock_metric.mock_name1.assert_called_once_with('arg1', kwarg1='value')
    mock_metric.mock_name2.assert_called_once_with('arg2', kwarg2='value')

  def testFlushingProcessExits(self):
    """Tests that _CreateTsMonFlushingProcess cleans up the process."""
    processes = []
    original_process_function = multiprocessing.Process
    def SaveProcess(*args, **kwargs):
      p = original_process_function(*args, **kwargs)
      processes.append(p)
      return p

    self.PatchObject(multiprocessing, 'Process', SaveProcess)

    with ts_mon_config._CreateTsMonFlushingProcess([], {}) as q:
      # We can't pickle a Mock() object, so just create any placeholder object.
      q.put((object(), '__class__', [], {}))

    # wait a bit for the process to close, since multiprocessing.Queue and
    # Process.join() is not synchronous.
    processes[0].join(5)

    self.assertEqual(0, processes[0].exitcode)

  def testCatchesException(self):
    """Tests that the _ConsumeMessages loop catches exceptions."""
    q = Queue.Queue()
    class RaisesException(object):
      """Class to raise an exception"""
      @staticmethod
      def raiseException(*_args, **_kwargs):
        raise Exception()
    q.put((RaisesException, 'raiseException', ['arg1'], {'kwarg1': 'value1'}))
    q.put(None)

    mock_logging = self.PatchObject(ts_mon_config.logging, 'exception')

    ts_mon_config._ConsumeMessages(q, [''], {})

    self.assertEqual(1, mock_logging.call_count)
    self.assertEqual(2, ts_mon_config.time.time.call_count)
    ts_mon_config.time.sleep.assert_called_once_with(
        ts_mon_config.FLUSH_INTERVAL - 1)
    ts_mon_config.metrics.flush.assert_called_once_with()
