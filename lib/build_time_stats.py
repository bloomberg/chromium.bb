# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that shows build timing for a build, and it's stages.

This script shows how long a build took, how long each stage took, and when each
stage started relative to the start of the build.
"""

from __future__ import print_function

import collections
import datetime
import itertools

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib


# MUST be kept in sync with GetParser's build-type option.
BUILD_TYPE_MAP = {
    'cq': constants.CQ_MASTER,
    'canary': constants.CANARY_MASTER,
    'chrome-pfq': constants.PFQ_MASTER,
}


BuildTiming = collections.namedtuple(
    'BuildTiming', ['id', 'build_config',
                    'start', 'finish', 'duration',
                    'stages'])


# Sometimes used with TimeDeltas, and sometimes with TimeDeltaStats.
StageTiming = collections.namedtuple(
    'StageTiming', ['name', 'start', 'finish', 'duration'])


class TimeDeltaStats(collections.namedtuple(
    'TimeDeltaStats', ['median', 'mean', 'min', 'max'])):
  """Collection a stats about a set of time.timedelta values."""
  __slotes__ = ()
  def __str__(self):
    return 'median %s mean %s min %s max %s' % self


def FillInBuildStatusesWithStages(db, build_statuses):
  """Fill in a 'stages' value for a list of build_statuses.

  Modifies the build_status objects in-place.

  Args:
    db: cidb.CIDBConnection object.
    build_statuses: List of build_status dictionaries as returned by various
                    CIDB methods.
  """
  ids = [status['id'] for status in build_statuses]
  all_stages = db.GetBuildsStages(ids)

  stages_by_build_id = cros_build_lib.GroupByKey(all_stages, 'build_id')

  for status in build_statuses:
    status['stages'] = stages_by_build_id.get(status['id'], [])


def BuildIdToBuildStatus(db, build_id):
  """Fetch a BuildStatus (with stages) from cidb from a build_id.

  Args:
    db: cidb.CIDBConnection object.
    build_id: build id as an integer.

  Returns:
    build status dictionary from CIDB with 'stages' field populated.
  """
  build_status = db.GetBuildStatus(build_id)
  build_status['stages'] = db.GetBuildStages(build_id)
  return build_status


def FilterBuildStatuses(build_statuses):
  """We only want to process passing 'normal' builds for stats.

  Args:
    build_statuses: List of Cidb result dictionary. 'stages' are not needed.

  Returns:
    List of all build statuses that weren't removed.
  """
  # Ignore tryserver, release branches, branch builders, chrome waterfall, etc.
  WATERFALLS = ('chromeos', 'chromiumos')

  return [status for status in build_statuses
          if status['status'] == 'pass' and status['waterfall'] in WATERFALLS]


def BuildConfigToStatuses(db, build_config, start_date, end_date):
  """Find a list of BuildStatus dictionaries with stages populated.

  Args:
    db: cidb.CIDBConnection object.
    build_config: Name of build config to find builds for.
    start_date: datetime.datetime object for start of search range.
    end_date: datetime.datetime object for end of search range.

  Returns:
    A list of cidb style BuildStatus dictionaries with 'stages' populated.
  """
  # Find builds.
  build_statuses = db.GetBuildHistory(
      build_config, db.NUM_RESULTS_NO_LIMIT,
      start_date=start_date, end_date=end_date)

  build_statuses = FilterBuildStatuses(build_statuses)

  # Fill in stage information.
  FillInBuildStatusesWithStages(db, build_statuses)
  return build_statuses


def MasterConfigToStatuses(db, build_config, start_date, end_date):
  """Find a list of BuildStatuses for all master/slave builds.

  Args:
    db: cidb.CIDBConnection object.
    build_config: Name of build config of master builder.
    start_date: datetime.datetime object for start of search range.
    end_date: datetime.datetime object for end of search range.

  Returns:
    A list of cidb style BuildStatus dictionaries with 'stages' populated.
  """
  # Find masters.
  master_statuses = db.GetBuildHistory(
      build_config, db.NUM_RESULTS_NO_LIMIT,
      start_date=start_date, end_date=end_date)

  # Find slaves.
  build_statuses = []
  for status in master_statuses:
    build_statuses += db.GetSlaveStatuses(status['id'])

  build_statuses = FilterBuildStatuses(build_statuses)

  # Fill in stage information.
  FillInBuildStatusesWithStages(db, build_statuses)
  return build_statuses


def GetBuildTimings(build_status):
  """Convert a build_status with stages into BuildTimings.

  After filling in a build_status dictionary with stage information
  (FillInBuildStatusesWithStages), convert to a BuildTimings tuple with only the
  data we care about.

  Args:
    build_status: Cidb result dictionary with 'stages' added.

  Returns:
    BuildTimings tuple with all time values populated as timedeltas or None.
  """
  start = build_status['start_time']

  def safeDuration(start, finish):
    # Do time math, but don't raise on a missing value.
    if start is None or finish is None:
      return None
    return finish - start

  stage_times = []
  for stage in build_status['stages']:
    stage_times.append(
        StageTiming(stage['name'],
                    safeDuration(start, stage['start_time']),
                    safeDuration(start, stage['finish_time']),
                    safeDuration(stage['start_time'], stage['finish_time'])))

  return BuildTiming(build_status['id'],
                     build_status['build_config'],
                     build_status['start_time'],
                     build_status['finish_time'],
                     safeDuration(start, build_status['finish_time']),
                     stage_times)


def CalculateTimeStats(durations):
  """Use a set of durations to populate a TimeDelaStats.

  Args:
    durations: A list of timedate.timedelta objects. May contain None values.

  Returns:
    A TimeDeltaStats object or None (if no valid deltas).
  """
  durations = [d for d in durations if d is not None]

  if not durations:
    return None

  durations.sort()
  median = durations[len(durations) / 2]

  # Convert to seconds so we can round to nearest second.
  summation = sum(d.total_seconds() for d in durations)
  average = datetime.timedelta(seconds=int(summation / len(durations)))

  minimum = durations[0]
  maximum = durations[-1]

  return TimeDeltaStats(median, average, minimum, maximum)


def CalculateBuildStats(builds_timings):
  """Find total build time stats for a set of BuildTiming objects.

  Args:
    builds_timings: List of BuildTiming objects.

  Returns:
    TimeDeltaStats object,or None if no valid builds.
  """
  return CalculateTimeStats([b.duration for b in builds_timings])


def CalculateStageStats(builds_timings):
  """Find time stats for all stages in a set of BuildTiming objects.

  Given a set of builds, find all unique stage names, and calculate average
  stats across all instances of that stage name.

  Given a list of 20 builds, if a stage is only in 2 of them, it's stats are
  only computed across the two instances.

  Args:
    builds_timings: List of BuildTiming objects.

  Returns:
    List of StageTiming objects with all time values populated with
    TimeDeltaStats values.
  """
  all_stages = list(itertools.chain(*[b.stages for b in builds_timings]))
  stage_names = set()
  stage_names.update([s.name for s in all_stages])

  stage_stats = []

  for name in stage_names:
    named_stages = [s for s in all_stages if s.name == name]
    stage_stats.append(
        StageTiming(
            name=name,
            start=CalculateTimeStats([s.start for s in named_stages]),
            finish=CalculateTimeStats([s.finish for s in named_stages]),
            duration=CalculateTimeStats([s.duration for s in named_stages])))

  return stage_stats


def FindAndSortStageStats(focus_build, builds_timings):
  """Return a list of stage names, sorted by median start time.

  Args:
    focus_build: BuildTiming object for a single build, or None.
    builds_timings: List of BuildTiming objects.

  Returns:
    tuple (
      [stage names as strings, sorted by start time.],
      {name: StageTiming from focus_build},
      {name: StageTiming with TimeDeltaStats},
    )
  """
  stage_stats = CalculateStageStats(builds_timings)

  # Map name to StageTiming.
  focus_stages = {}
  if focus_build:
    focus_stages = {s.name: s for s in focus_build.stages}
  stats_stages = {s.name: s for s in stage_stats}

  # Order the stage names to display, sorted by median start time.
  stage_names = list(set(focus_stages.keys() + stats_stages.keys()))
  def name_key(name):
    f, s = focus_stages.get(name), stats_stages.get(name)
    return s.start.median if s else f.start or datetime.timedelta()
  stage_names.sort(key=name_key)

  return stage_names, focus_stages, stats_stages


def Report(focus_build, builds_timings, stages=True):
  """Generate a report describing our stats.

  Args:
    focus_build: A BuildTiming object for a build to compare against stats.
    builds_timings: List of BuildTiming objects to display stats for.
    stages: Include a per-stage break down in the report.

  Returns:
    The generated report as a multi-line string.
  """
  result = ""

  build_stats = CalculateBuildStats(builds_timings)

  if focus_build:
    result += 'Focus build: %s\n' % focus_build.id

  if builds_timings:
    builds_timings.sort(key=lambda b: b.id)
    result += ('Averages for %s Builds: %s - %s\n' %
               (len(builds_timings),
                builds_timings[0].id,
                builds_timings[-1].id))

  result += '  Build Time:\n'
  if focus_build:
    result += '    %s\n' % focus_build.duration

  if build_stats:
    result += '    %s\n' % (build_stats,)

  result += '\n'

  if stages:
    stage_names, focus_stages, stats_stages = FindAndSortStageStats(
        focus_build, builds_timings)

    # Display info about each stage.
    for name in stage_names:
      result += '  %s:\n' % name
      f, s = focus_stages.get(name), stats_stages.get(name)
      result += '    start:    %s %s\n' % (f.start if f else '',
                                           s.start if s else '')
      result += '    duration: %s %s\n' % (f.duration if f else '',
                                           s.duration if s else '')
      result += '    finish:   %s %s\n' % (f.finish if f else '',
                                           s.finish if s else '')

  return result


def ReportTrendingStats(timings, stages=True):
  """Generate a report describing our stats over time.

  Describes build time stats for all timings, and for timings broken down by
  month. Does not describe stages.

  Args:
    timings: List of BuildTiming objects to display stats for.
    stages: Include stage information in the report (MUCH more verbose).

  Returns:
    The generated report as a multi-line string.
  """
  if not timings:
    return 'No builds found.'

  result = ''

  # Break builds into months.
  all_months = []
  month = 0
  month_timings = []
  for timing in timings:
    if timing.start.month == month:
      month_timings.append(timing)
    else:
      month = timing.start.month
      month_timings = [timing]
      all_months.append(month_timings)

  # Report stats for all builds found.
  over_all_stats = CalculateBuildStats(timings)

  result = '%s(%s) - %s(%s)\n' % (timings[0].start.date(), timings[0].id,
                                  timings[-1].start.date(), timings[-1].id)
  result += 'ALL:     %s\n' % (over_all_stats,)

  if stages:
    stage_names, _, stats_stages = FindAndSortStageStats(None, timings)

    for name in stage_names:
      result += '  %s:\n' % name
      s = stats_stages.get(name)
      result += '    start:    %s\n' % (s.start,)
      result += '    duration: %s\n' % (s.duration,)
      result += '    finish:   %s\n' % (s.finish,)


  # Report stats per month.
  for month_timings in all_months:
    month_stats = CalculateBuildStats(month_timings)
    prefix = '%s-%s:' % (month_timings[0].start.year,
                         month_timings[0].start.month)
    result += '%s%s\n' % (prefix.ljust(9), month_stats)

    if stages:
      stage_names, _, stats_stages = FindAndSortStageStats(None, month_timings)

      for name in stage_names:
        result += '  %s:\n' % name
        s = stats_stages.get(name)
        result += '    start:    %s\n' % (s.start,)
        result += '    duration: %s\n' % (s.duration,)
        result += '    finish:   %s\n' % (s.finish,)

  return result
