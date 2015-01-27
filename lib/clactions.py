# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for interacting with a CL's action history."""

from __future__ import print_function

import datetime
import itertools
import collections
from chromite.cbuildbot import constants


# Bidirectional mapping between pre-cq status strings and CL action strings.
_PRECQ_STATUS_TO_ACTION = {
    constants.CL_STATUS_INFLIGHT: constants.CL_ACTION_PRE_CQ_INFLIGHT,
    constants.CL_STATUS_FULLY_VERIFIED:
        constants.CL_ACTION_PRE_CQ_FULLY_VERIFIED,
    constants.CL_STATUS_PASSED: constants.CL_ACTION_PRE_CQ_PASSED,
    constants.CL_STATUS_FAILED: constants.CL_ACTION_PRE_CQ_FAILED,
    constants.CL_STATUS_LAUNCHING: constants.CL_ACTION_PRE_CQ_LAUNCHING,
    constants.CL_STATUS_WAITING: constants.CL_ACTION_PRE_CQ_WAITING,
    constants.CL_STATUS_READY_TO_SUBMIT:
        constants.CL_ACTION_PRE_CQ_READY_TO_SUBMIT
}

_PRECQ_ACTION_TO_STATUS = dict(
    (v, k) for k, v in _PRECQ_STATUS_TO_ACTION.items())

assert len(_PRECQ_STATUS_TO_ACTION) == len(_PRECQ_ACTION_TO_STATUS), \
    '_PRECQ_STATUS_TO_ACTION values are not unique.'

CL_ACTION_COLUMNS = ['id', 'build_id', 'action', 'reason',
                     'build_config', 'change_number', 'patch_number',
                     'change_source', 'timestamp']

_CLActionTuple = collections.namedtuple('_CLActionTuple', CL_ACTION_COLUMNS)

_GerritPatchTuple = collections.namedtuple('_GerritPatchTuple',
                                           ['gerrit_number', 'patch_number',
                                            'internal'])

class GerritPatchTuple(_GerritPatchTuple):
  """A tuple for a given Gerrit patch."""

  def __str__(self):
    prefix = (constants.INTERNAL_CHANGE_PREFIX
              if self.internal else constants.EXTERNAL_CHANGE_PREFIX)
    return 'CL:%s%s#%s' % (prefix, self.gerrit_number, self.patch_number)


_GerritChangeTuple = collections.namedtuple('_GerritChangeTuple',
                                            ['gerrit_number', 'internal'])


class GerritChangeTuple(_GerritChangeTuple):
  """A tuple for a given Gerrit change."""

  def __str__(self):
    prefix = (constants.INTERNAL_CHANGE_PREFIX
              if self.internal else constants.EXTERNAL_CHANGE_PREFIX)
    return 'CL:%s%s' % (prefix, self.gerrit_number)


class CLAction(_CLActionTuple):
  """An action or history log entry for a particular CL."""

  @classmethod
  def FromGerritPatchAndAction(cls, change, action, reason=None,
                               timestamp=None):
    """Creates a CLAction instance from a change and action.

    Args:
      change: A GerritPatch instance.
      action: An action string.
      reason: Optional reason string.
      timestamp: Optional datetime.datetime timestamp.
    """
    return CLAction(None, None, action, reason, None,
                    int(change.gerrit_number), int(change.patch_number),
                    BoolToChangeSource(change.internal), timestamp)

  @classmethod
  def FromMetadataEntry(cls, entry):
    """Creates a CLAction instance from a metadata.json-style action tuple.

    Args:
      entry: An action tuple as retrieved from metadata.json (previously known
             as a CLActionTuple).
      build_metadata: The full build metadata.json entry.
    """
    change_dict = entry[0]
    return CLAction(None, None, entry[1], entry[3], None,
                    int(change_dict['gerrit_number']),
                    int(change_dict['patch_number']),
                    BoolToChangeSource(change_dict['internal']),
                    entry[2])

  def AsMetadataEntry(self):
    """Get a tuple representation, suitable for metadata.json."""
    return (self.patch._asdict(), self.action, self.timestamp, self.reason)

  @property
  def patch(self):
    """The GerritPatch this action affects."""
    return GerritPatchTuple(
        gerrit_number=self.change_number,
        patch_number=self.patch_number,
        internal=self.change_source == constants.CHANGE_SOURCE_INTERNAL
    )


def TranslatePreCQStatusToAction(status):
  """Translate a pre-cq |status| into a cl action.

  Returns:
    An action string suitable for use in cidb, for the given pre-cq status.

  Raises:
    KeyError if |status| is not a known pre-cq status.
  """
  return _PRECQ_STATUS_TO_ACTION[status]


def TranslatePreCQActionToStatus(action):
  """Translate a cl |action| into a pre-cq status.

  Returns:
    A pre-cq status string corresponding to the given |action|.

  Raises:
    KeyError if |action| is not a known pre-cq status-transition-action.
  """
  return _PRECQ_ACTION_TO_STATUS[action]


def BoolToChangeSource(internal):
  """Translate a change.internal bool into a change_source string.

  Returns:
    'internal' if internal, else 'external'.
  """
  return (constants.CHANGE_SOURCE_INTERNAL if internal
          else constants.CHANGE_SOURCE_EXTERNAL)


def GetCLPreCQStatusAndTime(change, action_history):
  """Get the pre-cq status and timestamp for |change| from |action_history|.

  Args:
    change: GerritPatch instance to get the pre-CQ status for.
    action_history: A list of CLAction instances, which may include actions
                    for other changes.

  Returns:
    A (status, timestamp) tuple where |status| is a valid pre-cq status
    string and |timestamp| is a datetime object for when the status was
    set. Or (None, None) if there is no pre-cq status.
  """
  actions_for_patch = ActionsForPatch(change, action_history)
  actions_for_patch = [
      a for a in actions_for_patch if a.action in _PRECQ_ACTION_TO_STATUS or
      a.action == constants.CL_ACTION_PRE_CQ_RESET]

  if (not actions_for_patch or
      actions_for_patch[-1].action == constants.CL_ACTION_PRE_CQ_RESET):
    return None, None

  return (TranslatePreCQActionToStatus(actions_for_patch[-1].action),
          actions_for_patch[-1].timestamp)


def GetCLPreCQStatus(change, action_history):
  """Get the pre-cq status for |change| based on |action_history|.

  Args:
    change: GerritPatch instance to get the pre-CQ status for.
    action_history: A list of CLAction instances. This may include
                    actions for changes other than |change|.

  Returns:
    The status, as a string, or None if there is no recorded pre-cq status.
  """
  return GetCLPreCQStatusAndTime(change, action_history)[0]


def IsChangeScreened(change, action_history):
  """Get's whether |change| has been pre-cq screened.

  Args:
    change: GerritPatch instance to get the pre-CQ status for.
    action_history: A list of CLAction instances.

  Returns:
    True if the change has been pre-cq screened, false otherwise.
  """
  actions_for_patch = ActionsForPatch(change, action_history)
  actions_for_patch = FilterPreResetActions(actions_for_patch)
  return any(a.action == constants.CL_ACTION_SCREENED_FOR_PRE_CQ
             for a in actions_for_patch)


def ActionsForPatch(change, action_history):
  """Filters a CL action list to only those for a given patch.

  Args:
    change: GerritPatch instance to filter for.
    action_history: List of CLAction objects.
  """
  patch_number = int(change.patch_number)
  change_number = int(change.gerrit_number)
  change_source = BoolToChangeSource(change.internal)

  actions_for_patch = [a for a in action_history
                       if (a.change_source == change_source and
                           a.change_number == change_number and
                           a.patch_number == patch_number)]

  return actions_for_patch


def GetRequeuedOrSpeculative(change, action_history, is_speculative):
  """For a |change| get either a requeued or speculative action if necessary.

  This method returns an action string for an action that should be recorded
  on |change|, or None if no action needs to be recorded.

  Args:
    change: GerritPatch instance to operate upon.
    action_history: List of CL actions (may include actions on changes other
                    than |change|).
    is_speculative: Boolean indicating if |change| is speculative, i.e. it does
                    not have CQ approval.

  Returns:
    CL_ACTION_REQUEUED, CL_ACTION_SPECULATIVE, or None.
  """
  actions_for_patch = ActionsForPatch(change, action_history)

  if is_speculative:
    # Speculative changes should have 1 CL_ACTION_SPECULATIVE action that is
    # newer than the newest REQUEUED or KICKED_OUT action, and at least 1
    # action if there is no REQUEUED or KICKED_OUT action.
    for a in reversed(actions_for_patch):
      if a.action == constants.CL_ACTION_SPECULATIVE:
        return None
      elif (a.action == constants.CL_ACTION_REQUEUED or
            a.action == constants.CL_ACTION_KICKED_OUT):
        return constants.CL_ACTION_SPECULATIVE
    return constants.CL_ACTION_SPECULATIVE
  else:
    # Non speculative changes should have 1 CL_ACTION_REQUEUED action that is
    # newer than the newest SPECULATIVE or KICKED_OUT action, but no action if
    # there are no SPECULATIVE or REQUEUED actions.
    for a in reversed(actions_for_patch):
      if (a.action == constants.CL_ACTION_KICKED_OUT or
          a.action == constants.CL_ACTION_SPECULATIVE):
        return constants.CL_ACTION_REQUEUED
      if a.action == constants.CL_ACTION_REQUEUED:
        return None

  return None


def GetCLActionCount(change, configs, action, action_history,
                     latest_patchset_only=True):
  """Return how many times |action| has occured on |change|.

  Args:
    change: GerritPatch instance to operate upon.
    configs: List or set of config names to consider.
    action: The action string to look for.
    action_history: List of CLAction instances to count through.
    latest_patchset_only: If True, only count actions that occured to the
      latest patch number. Note, this may be different than the patch
      number specified in |change|. Default: True.

  Returns:
    The count of how many times |action| occured on |change| by the given
    |config|.
  """
  change_number = int(change.gerrit_number)
  change_source = BoolToChangeSource(change.internal)
  actions_for_change = [a for a in action_history
                        if (a.change_source == change_source and
                            a.change_number == change_number)]

  if actions_for_change and latest_patchset_only:
    latest_patch_number = max(a.patch_number for a in actions_for_change)
    actions_for_change = [a for a in actions_for_change
                          if a.patch_number == latest_patch_number]

  actions_for_change = [a for a in actions_for_change
                        if (a.build_config in configs and
                            a.action == action)]

  return len(actions_for_change)


def FilterPreResetActions(action_history):
  """Filters out actions prior to most recent pre-cq reset action.

  Args:
    action_history: List of CLAction instance.

  Returns:
    List of CLAction instances that occur after the last pre-cq-reset action.
  """
  reset = False
  for i, a in enumerate(action_history):
    if a.action == constants.CL_ACTION_PRE_CQ_RESET:
      reset = True
      reset_index = i
  if reset:
    action_history = action_history[(reset_index+1):]
  return action_history


def GetCLPreCQProgress(change, action_history):
  """Gets a CL's per-config PreCQ statuses.

  Args:
    change: GerritPatch instance to get statuses for.
    action_history: List of CLAction instances.

  Returns:
    A dict of the form {config_name: (status, timestamp, build_id)} specifying
    all the per-config pre-cq statuses, where status is one of
    constants.CL_PRECQ_CONFIG_STATUSES, timestamp is a datetime.datetime of
    when this status was most recently achieved, and build_id is the id of the
    build which most recently updated this per-config status.
  """
  actions_for_patch = ActionsForPatch(change, action_history)
  config_status_dict = {}

  # If there is a reset action recorded, filter out all actions prior to it.
  actions_for_patch = FilterPreResetActions(actions_for_patch)

  # Only configs for which the pre-cq-launcher has requested verification
  # should be included in the per-config status.
  for a in actions_for_patch:
    if a.action == constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ:
      assert a.reason, 'Validation was requested without a specified config.'
      config_status_dict[a.reason] = (constants.CL_PRECQ_CONFIG_STATUS_PENDING,
                                      a.timestamp, a.build_id)

  # Loop through actions_for_patch several times, in order of status priority.
  # Each action maps to a status:
  #   CL_ACTION_TRYBOT_LAUNCHING -> CL_PRECQ_CONFIG_STATUS_LAUNCHED
  #   CL_ACTION_PICKED_UP -> CL_PRECQ_CONFIG_STATUS_INFLIGHT
  #   CL_ACTION_KICKED_OUT -> CL_PRECQ_CONFIG_STATUS_FAILED
  #   CL_ACTION_FORGIVEN -> CL_PRECQ_CONFIG_STATUS_PENDING
  # All have the same priority.
  for a in actions_for_patch:
    if (a.action == constants.CL_ACTION_TRYBOT_LAUNCHING and
        a.reason in config_status_dict):
      config_status_dict[a.reason] = (constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED,
                                      a.timestamp, a.build_id)
    elif (a.action == constants.CL_ACTION_PICKED_UP and
          a.build_config in config_status_dict):
      config_status_dict[a.build_config] = (
          constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT, a.timestamp, a.build_id)
    elif (a.action == constants.CL_ACTION_KICKED_OUT and
          (a.build_config in config_status_dict or
           a.reason in config_status_dict)):
      config = (a.build_config if a.build_config in config_status_dict else
                a.reason)
      config_status_dict[config] = (constants.CL_PRECQ_CONFIG_STATUS_FAILED,
                                    a.timestamp, a.build_id)
    elif (a.action == constants.CL_ACTION_FORGIVEN and
          (a.build_config in config_status_dict or
           a.reason in config_status_dict)):
      config = (a.build_config if a.build_config in config_status_dict else
                a.reason)
      config_status_dict[config] = (constants.CL_PRECQ_CONFIG_STATUS_PENDING,
                                    a.timestamp, a.build_id)

  for a in actions_for_patch:
    if (a.action == constants.CL_ACTION_VERIFIED and
        a.build_config in config_status_dict):
      config_status_dict[a.build_config] = (
          constants.CL_PRECQ_CONFIG_STATUS_VERIFIED, a.timestamp, a.build_id)

  return config_status_dict


def GetPreCQProgressMap(changes, action_history):
  """Gets the per-config pre-cq status for all changes.

  Args:
    changes: Set of GerritPatch changes to consider.
    action_history: List of CLAction instances.

  Returns:
    A dict of the form {change: config_status_dict} where config_status_dict
    is as returned by GetCLPreCQProgress. Any change that has not yet been
    screened will be absent from the returned dict.
  """
  progress_map = {}
  for change in changes:
    config_status_dict = GetCLPreCQProgress(change, action_history)
    if config_status_dict:
      progress_map[change] = config_status_dict

  return progress_map


def GetPreCQCategories(progress_map):
  """Gets the set of busy and verified CLs in the pre-cq.

  Args:
    progress_map: See return type of GetPreCQProgressMap.

  Returns:
    A (busy, inflight, verified) tuple where each item is a set of changes.
    A change is verified if all its pending configs have verified it. A change
    is busy if it is not verified, but all pending configs are either launched
    or inflight or verified. A change is inflight if all configs are at least
    at or past the inflight state, and at least one config is still inflight.
  """
  busy, inflight, verified = set(), set(), set()
  busy_states = (constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED,
                 constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT,
                 constants.CL_PRECQ_CONFIG_STATUS_VERIFIED)
  beyond_inflight_states = (constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT,
                            constants.CL_PRECQ_CONFIG_STATUS_VERIFIED,
                            constants.CL_PRECQ_CONFIG_STATUS_FAILED)

  for change, config_status_dict in progress_map.iteritems():
    statuses = [x for x, _, _, in config_status_dict.values()]
    if all(x == constants.CL_PRECQ_CONFIG_STATUS_VERIFIED for x in statuses):
      verified.add(change)
    elif all(x in busy_states for x in statuses):
      busy.add(change)

    if (all(x in beyond_inflight_states for x in statuses) and
        any(x == constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT for x in statuses)):
      inflight.add(change)

  return busy, inflight, verified


def GetPreCQConfigsToTest(changes, progress_map):
  """Gets the set of configs to be tested for any change in |changes|.

  Note: All |changes| must already be screened, i.e. must appear in
  progress_map.

  Args:
    changes: A list or set of changes (GerritPatch).
    progress_map: See return type of GetPreCQProgressMap.

  Returns:
    A set of configs that must be launched in order to make each change in
    |changes| be considered 'busy' by the pre-cq.

  Raises:
    KeyError if any change in |changes| is not yet screened, and hence
    does not appear in progress_map.
  """
  configs_to_test = set()
  # Failed is considered a to-test state so that if a CL fails a given config
  # and gets rejected, it will be re-tested by that config when it is re-queued.
  to_test_states = (constants.CL_PRECQ_CONFIG_STATUS_PENDING,
                    constants.CL_PRECQ_CONFIG_STATUS_FAILED)
  for change in changes:
    for config, (status, _, _) in progress_map[change].iteritems():
      if status in to_test_states:
        configs_to_test.add(config)
  return configs_to_test


def IntersectIntervals(intervals):
  """Gets the intersection of a set of intervals.

  Args:
    intervals: A list of interval groups, where each interval group is itself
               a list of (start, stop) tuples (ordered by start time and
               non-overlapping).

  Returns:
    An interval group, as a list of (start, stop) tuples, corresponding to the
    intersection (i.e. overlap) of the given |intervals|.
  """
  if not intervals:
    return []

  intersection = []
  indices = [0] * len(intervals)
  lengths = [len(i) for i in intervals]
  while all(i < l for i, l in zip(indices, lengths)):
    current_intervals = [intervals[i][j] for (i, j) in
                         zip(itertools.count(), indices)]
    start = max([s[0] for s in current_intervals])
    end, end_index = min([(e[1], i) for e, i in
                          zip(current_intervals, itertools.count())])
    if start < end:
      intersection.append((start, end))
    indices[end_index] += 1

  return intersection


def MeasureTimestampIntervals(intervals):
  """Gets the length of a set of invervals.

  Args:
    intervals: A list of (start, stop) timestamp tuples.

  Returns:
    The total length of the given intervals, in seconds.
  """
  lengths = [e - s for s, e in intervals]
  return sum(lengths, datetime.timedelta(0)).total_seconds()


def GetIntervals(change, action_history, start_actions, stop_actions,
                 start_at_beginning=False):
  """Get intervals corresponding to given start and stop actions.

  Args:
    change: GerritPatch instance of a submitted change.
    action_history: list of CL actions.
    start_actions: list of action types to be considered as start actions for
                   intervals.
    stop_actions: list of action types to be considered as stop actions for
                  intervals.
    start_at_beginning: optional boolean, default False. If true, consider the
                        first action to be a start action.
  """
  actions_for_patch = ActionsForPatch(change, action_history)
  if not actions_for_patch:
    return []

  intervals = []
  in_interval = start_at_beginning
  if in_interval:
    start_time = actions_for_patch[0].timestamp
  for a in actions_for_patch:
    if in_interval and a.action in stop_actions:
      if start_time < a.timestamp:
        intervals.append((start_time, a.timestamp))
      in_interval = False
    elif not in_interval and a.action in start_actions:
      start_time = a.timestamp
      in_interval = True

  if in_interval and start_time < actions_for_patch[-1].timestamp:
    intervals.append((start_time, actions_for_patch[-1].timestamp))

  return intervals


def GetReadyIntervals(change, action_history):
  """Gets the time intervals in which |change| was fully ready.

  Args:
    change: GerritPatch instance of a submitted change.
    action_history: list of CL actions.
  """
  start = (constants.CL_ACTION_REQUEUED,)
  stop = (constants.CL_ACTION_SPECULATIVE, constants.CL_ACTION_KICKED_OUT)
  return GetIntervals(change, action_history, start, stop, True)


def GetCLHandlingTime(change, action_history):
  """Returns the handling time of |change|, in seconds.

  This method computes a CL's handling time, not including the time spent
  waiting for a developer to mark or re-mark their change as ready.

  Args:
    change: GerritPatch instance of a submitted change.
    action_history: List of CL actions.
  """
  ready_intervals = GetReadyIntervals(change, action_history)
  return MeasureTimestampIntervals(ready_intervals)


def GetPreCQTime(change, action_history):
  """Returns the time spent waiting for the pre-cq to finish."""
  ready_intervals = GetReadyIntervals(change, action_history)
  start = (constants.CL_ACTION_SCREENED_FOR_PRE_CQ,)
  stop = (constants.CL_ACTION_PRE_CQ_FULLY_VERIFIED,)
  precq_intervals = GetIntervals(change, action_history, start, stop)
  return MeasureTimestampIntervals(
      IntersectIntervals([ready_intervals, precq_intervals]))


def GetCQWaitTime(change, action_history):
  """Returns the time spent waiting for a CL to be picked up by the CQ."""
  ready_intervals = GetReadyIntervals(change, action_history)
  precq_passed_interval = GetIntervals(
      change, action_history, (constants.CL_ACTION_PRE_CQ_PASSED,), ())
  relevant_configs = (constants.PRE_CQ_LAUNCHER_CONFIG, constants.CQ_MASTER)
  relevant_config_actions = [a for a in action_history
                             if a.build_config in relevant_configs]
  start = (constants.CL_ACTION_REQUEUED, constants.CL_ACTION_FORGIVEN)
  stop = (constants.CL_ACTION_PICKED_UP,)
  waiting_intervals = GetIntervals(change, relevant_config_actions, start, stop,
                                   True)
  return MeasureTimestampIntervals(
      IntersectIntervals([ready_intervals, waiting_intervals,
                          precq_passed_interval]))


def GetCQRunTime(change, action_history):
  """Returns the time spent testing a CL in the CQ."""
  ready_intervals = GetReadyIntervals(change, action_history)
  relevant_configs = (constants.CQ_MASTER,)
  relevant_config_actions = [a for a in action_history
                             if a.build_config in relevant_configs]
  start = (constants.CL_ACTION_PICKED_UP,)
  stop = (constants.CL_ACTION_FORGIVEN, constants.CL_ACTION_KICKED_OUT,
          constants.CL_ACTION_SUBMITTED)
  testing_intervals = GetIntervals(change, relevant_config_actions, start, stop)
  return MeasureTimestampIntervals(
      IntersectIntervals([ready_intervals, testing_intervals]))


def GetRelevantChangesForBuilds(changes, action_history, build_ids):
  """Get relevant changes for |build_ids| by examing CL actions.

  Args:
    changes: A list of GerritPatch instances to examine.
    action_history: A list of CLAction instances.
    build_ids: A list of build id to examine.

  Returns:
    A dictionary mapping a build id to a set of changes.
  """
  changes_map = dict()
  relevant_actions = [x for x in action_history if x.build_id in build_ids]
  for change in changes:
    actions = ActionsForPatch(change, relevant_actions)
    pickups = set([x.build_id for x in actions if
                   x.action == constants.CL_ACTION_PICKED_UP])
    discards = set([x.build_id for x in actions if
                    x.action == constants.CL_ACTION_IRRELEVANT_TO_SLAVE])
    relevant_build_ids = pickups - discards
    for build_id in relevant_build_ids:
      changes_map.setdefault(build_id, set()).add(change)

  return changes_map
