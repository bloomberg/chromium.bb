# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for statsd mock."""

from __future__ import print_function

import unittest

from chromite.lib.graphite_lib import statsd_mock as statsd

class statsd_mock_test(unittest.TestCase):
  """Test statsd_mock"""
  def test_average_mock(self):
    """Test mock class Average"""
    statsd.Average('average').send('name', 1)


  def test_connection_mock(self):
    """Test mock class Connection"""
    statsd.Connection(host='host', port=1)
    statsd.Connection.set_defaults(host='host', port=1)


  def test_counter_mock(self):
    """Test mock class Counter"""
    counter = statsd.Counter('counter')
    counter.increment(subname='name', delta=1)
    counter.decrement(subname='name', delta=1)


  def test_gauge_mock(self):
    """Test mock class Gauge"""
    statsd.Gauge('gauge').send('name', 1)


  def test_raw_mock(self):
    """Test mock class Raw"""
    statsd.Raw('raw').send(subname='name', value=1, timestamp=None)


  def test_timer_mock(self):
    """Test mock class Timer"""
    timer = statsd.Timer('timer')
    timer.start()
    timer.stop()

    class decorate_test(object):
      """Test class to test timer decorator."""
      test_timer = statsd.Timer('test')

      @test_timer.decorate
      def f(self):
        """Test function to apply timer decorator to."""
        return True

    dt = decorate_test()
    self.assertTrue(dt.f(), 'timer decorator failed.')

if __name__ == '__main__':
  unittest.main()
