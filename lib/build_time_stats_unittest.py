# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for boolparse_lib methods."""

from __future__ import print_function

import datetime

import build_time_stats
import cros_test_lib


class BuildTimeStatsTest(cros_test_lib.TestCase):
  """Unittest build_time_stats code that doesn't use the database."""

  def testFilterBuildStatuses(self):
    test_data = [
        {'status': 'pass', 'waterfall': 'chromeos'},
        {'status': 'fail', 'waterfall': 'chromeos'},
        {'status': 'pass', 'waterfall': 'chromiumos'},
        {'status': 'fail', 'waterfall': 'chromiumos'},
        {'status': 'pass', 'waterfall': 'chromiumos.tryserver'},
        {'status': 'fail', 'waterfall': 'chromiumos.tryserver'},
        {'status': 'pass', 'waterfall': 'bogus'},
        {'status': 'fail', 'waterfall': 'bogus'},
    ]

    expected = [
        {'status': 'pass', 'waterfall': 'chromeos'},
        {'status': 'pass', 'waterfall': 'chromiumos'},
    ]

    result = build_time_stats.FilterBuildStatuses(test_data)
    self.assertEqual(result, expected)

  def testGetBuildTimings(self):

    startTime = datetime.datetime(2014, 11, 3, 16, 22)

    good_data = {
        'id': 1,
        'build_config': 'test_config',
        'start_time': startTime,
        'finish_time': startTime + datetime.timedelta(hours=6),
        'stages': [
            {
                'name': 'start',
                'start_time': startTime,
                'finish_time': startTime + datetime.timedelta(hours=2),
            },
            {
                'name': 'build',
                'start_time': startTime + datetime.timedelta(hours=2),
                'finish_time': startTime + datetime.timedelta(hours=4),
            },
        ],
    }

    expected_good = build_time_stats.BuildTiming(
        id=1,
        build_config='test_config',
        duration=datetime.timedelta(hours=6),
        stages=[
            build_time_stats.StageTiming(
                name='start',
                start=datetime.timedelta(0),
                finish=datetime.timedelta(hours=2),
                duration=datetime.timedelta(hours=2)),
            build_time_stats.StageTiming(
                name='build',
                start=datetime.timedelta(hours=2),
                finish=datetime.timedelta(hours=4),
                duration=datetime.timedelta(hours=2))
        ]
    )

    bad_data = {
        'id': 2,
        'build_config': 'test_config',
        'start_time': startTime,
        'finish_time': None,
        'stages': [
            {
                'name': 'start',
                'start_time': startTime,
                'finish_time': None,
            },
            {
                'name': 'build',
                'start_time': None,
                'finish_time': startTime + datetime.timedelta(hours=4),
            },
        ],
    }

    expected_bad = build_time_stats.BuildTiming(
        id=2,
        build_config='test_config',
        duration=None,
        stages=[
            build_time_stats.StageTiming(
                name='start',
                start=datetime.timedelta(0),
                finish=None,
                duration=None),
            build_time_stats.StageTiming(
                name='build',
                start=None,
                finish=datetime.timedelta(hours=4),
                duration=None),
        ]
    )

    result = build_time_stats.GetBuildTimings(good_data)
    self.assertEqual(result, expected_good)

    result = build_time_stats.GetBuildTimings(bad_data)
    self.assertEqual(result, expected_bad)

  def testCalculateTimeStats(self):
    test_data = [
        None,
        datetime.timedelta(hours=0),
        None,
        datetime.timedelta(hours=2),
        datetime.timedelta(hours=7),
        None,
    ]

    expected = build_time_stats.TimeDeltaStats(
        median=datetime.timedelta(hours=2),
        mean=datetime.timedelta(hours=3),
        min=datetime.timedelta(0),
        max=datetime.timedelta(hours=7))

    result = build_time_stats.CalculateTimeStats(test_data)
    self.assertEqual(result, expected)

  def testCalculateTimeStatsNoInput(self):
    self.assertIsNone(build_time_stats.CalculateTimeStats([]))
    self.assertIsNone(build_time_stats.CalculateTimeStats([None, None]))

  def testCalculateStageStats(self):
    """TODO(dgarrett)"""

  def testReport(self):
    """TODO(dgarrett)"""
