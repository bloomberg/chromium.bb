# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Infrastructure for collecting statistics about retries."""

from __future__ import print_function

import collections

from chromite.lib import parallel
from chromite.lib import retry_util


GSUTIL = 'Google Storage'


CATEGORIES = frozenset([
  GSUTIL,
])


class UnconfiguredStatsCategory(Exception):
  """We tried to use a Stats Category without configuring it."""


# Always hold the lock before adjusting any other values.
StatValue = collections.namedtuple(
    'StatValue',
    ('lock', 'success', 'failure', 'retry'))


# map[category_name]StatValue
_STATS_COLLECTION = {}


def _ResetStatsForUnittests():
  """Helper for test code. Resets our global state."""
  _STATS_COLLECTION.clear()


def SetupStats(category_list=CATEGORIES):
  """Prepare a given category to collect stats.

  This must be called BEFORE any new processes that might read or write to
  these stat values are created. It is safe to call this more than once,
  but most efficient to only make a single call.

  Args:
    category_list: Iterable of categories to collect stats for.
  """
  # Pylint think our manager has no members.
  # pylint: disable=E1101
  m = parallel.Manager()

  # Create a new stats collection structure that is multiprocess usable.
  for category in category_list:
    lock = m.RLock()

    _STATS_COLLECTION[category] = StatValue(
        lock,
        m.Value('i', 0, lock),
        m.Value('i', 0, lock),
        m.Value('i', 0, lock),
    )


def ReportCategoryStats(out, category):
  """Dump stats reports for a given category.

  Args:
    out: Output stream to write to (e.g. sys.stdout).
    category: A string that defines the 'namespace' for these stats.
  """
  if category not in _STATS_COLLECTION:
    raise UnconfiguredStatsCategory('%s not configured before use' % category)

  stats = _STATS_COLLECTION[category]

  line = '*' * 60 + '\n'
  edge = '*' * 2

  success = stats.success.value
  failure = stats.failure.value
  retry = stats.retry.value

  out.write(line)
  out.write(edge + ' Performance Statistics for %s' % category + '\n')
  out.write(edge + '\n')
  out.write(edge + ' Success: %d' % success + '\n')
  out.write(edge + ' Failure: %d' % failure + '\n')
  out.write(edge + ' Retries: %d' % retry + '\n')
  out.write(edge + ' Total: %d' % (success + failure) + '\n')
  out.write(line)

def ReportStats(out):
  """Dump stats reports for a given category.

  Args:
    out: Output stream to write to (e.g. sys.stdout).
    category: A string that defines the 'namespace' for these stats.
  """
  categories = _STATS_COLLECTION.keys()
  categories.sort()

  for category in categories:
    ReportCategoryStats(out, category)


def RetryWithStats(category, handler, max_retry, functor, *args, **kwargs):
  """Wrapper around retry_util.GenericRetry that collects stats.

  This wrapper collects statistics about each failure or retry. Each
  category is defined by a unique string. Each category should be setup
  before use (actually, before processes are forked).

  All other arguments are blindly passed to retry_util.GenericRetry.

  Args:
    category: A string that defines the 'namespace' for these stats.
    handler: See retry_util.GenericRetry.
    max_retry: See retry_util.GenericRetry.
    functor: See retry_util.GenericRetry.
    args: See retry_util.GenericRetry.
    kwargs: See retry_util.GenericRetry.

  Returns:
    See retry_util.GenericRetry raises.

  Raises:
    See retry_util.GenericRetry raises.
  """
  stats = _STATS_COLLECTION.get(category, None)
  failures = []

  # Wrap the work method, so we can gather info.
  def wrapper(*args, **kwargs):
    try:
      return functor(*args, **kwargs)
    except Exception as e:
      failures.append(e)
      raise

  try:
    result = retry_util.GenericRetry(handler, max_retry, wrapper,
                                     *args, **kwargs)
  except Exception:
    if stats:
      with stats.lock:
        stats.retry.value += (len(failures) - 1)
        stats.failure.value += 1
    raise

  if stats:
    with stats.lock:
      stats.retry.value += len(failures)
      stats.success.value += 1

  return result

