# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for interacting with a CL's action history."""

from __future__ import print_function

import collections
from chromite.cbuildbot import constants

# Bidirectional mapping between pre-cq status strings and CL action strings.
_PRECQ_STATUS_TO_ACTION = {
    constants.CL_STATUS_INFLIGHT: constants.CL_ACTION_PRE_CQ_INFLIGHT,
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


#pylint: disable-msg=E1101,W0232
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
    """
    change_dict = entry[0]
    return CLAction(None, None, entry[1], entry[3],
                    None, int(change_dict['gerrit_number']),
                    int(change_dict['patch_number']),
                    BoolToChangeSource(change_dict['internal']),
                    entry[2])


  def AsMetadataEntry(self):
    """Get a tuple representation, suitable for metadata.json."""
    change_dict = {
        'gerrit_number': self.change_number,
        'patch_number': self.patch_number,
        'internal': self.change_source == constants.CHANGE_SOURCE_INTERNAL}
    return (change_dict, self.action, self.timestamp, self.reason)


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


def GetCLPreCQStatus(change, action_history):
  """Get the pre-cq status for |change| based on |action_history|.

  Args:
    change: GerritPatch instance to get the pre-CQ status for.
    action_history: A list of CLAction instances. This may include
                    actions for changes other than |change|.

  Returns:
    The status, as a string, or None if there is no recorded pre-cq status.
  """
  patch_number = int(change.patch_number)
  change_number = int(change.gerrit_number)
  change_source = BoolToChangeSource(change.internal)

  # Filter out actions to other patch numbers and actions that are not
  # pre-cq status actions.
  actions_for_patch = [a for a in action_history
                       if a.change_source == change_source and
                          a.change_number == change_number and
                          a.patch_number == patch_number and
                          a.action in _PRECQ_ACTION_TO_STATUS]

  if not actions_for_patch:
    return None

  return TranslatePreCQActionToStatus(actions_for_patch[-1].action)

