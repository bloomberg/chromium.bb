# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to manage stage failure messages."""

from __future__ import print_function

import collections
import json
import re

from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import hwtest_results
from chromite.lib import patch as cros_patch
from chromite.lib import portage_util
from chromite.lib import triage_lib


# These keys must exist as column names from failureView in cidb.
FAILURE_KEYS = (
    'id', 'build_stage_id', 'outer_failure_id', 'exception_type',
    'exception_message', 'exception_category', 'extra_info',
    'timestamp', 'stage_name', 'board', 'stage_status', 'build_id',
    'master_build_id', 'builder_name', 'waterfall', 'build_number',
    'build_config', 'build_status', 'important', 'buildbucket_id')


# A namedtuple containing values fetched from CIDB failureView.
_StageFailure = collections.namedtuple('_StageFailure', FAILURE_KEYS)


class StageFailure(_StageFailure):
  """A class presenting values of a failure fetched from CIDB failureView."""

  @classmethod
  def GetStageFailureFromMessage(cls, stage_failure_message):
    """Create StageFailure from a StageFailureMessage instance.

    Args:
      stage_failure_message: An instance of StageFailureMessage.

    Returns:
      An instance of StageFailure.
    """
    return StageFailure(
        stage_failure_message.failure_id,
        stage_failure_message.build_stage_id,
        stage_failure_message.outer_failure_id,
        stage_failure_message.exception_type,
        stage_failure_message.exception_message,
        stage_failure_message.exception_category,
        stage_failure_message.extra_info, None,
        stage_failure_message.stage_name, None, None, None, None, None, None,
        None, None, None, None, None)

  @classmethod
  def GetStageFailureFromDicts(cls, failure_dict, stage_dict, build_dict):
    """Get StageFailure from value dictionaries.

    Args:
      failure_dict: A dict presenting values of a tuple from failureTable.
      stage_dict: A dict presenting values of a tuple from buildStageTable.
      build_dict: A dict presenting values of a tuple from buildTable.

    Returns:
      An instance of StageFailure.
    """
    return StageFailure(
        failure_dict['id'], failure_dict['build_stage_id'],
        failure_dict['outer_failure_id'], failure_dict['exception_type'],
        failure_dict['exception_message'], failure_dict['exception_category'],
        failure_dict['extra_info'], failure_dict['timestamp'],
        stage_dict['name'], stage_dict['board'], stage_dict['status'],
        build_dict['id'], build_dict['master_build_id'],
        build_dict['builder_name'], build_dict['waterfall'],
        build_dict['build_number'], build_dict['build_config'],
        build_dict['status'], build_dict['important'],
        build_dict['buildbucket_id'])


class StageFailureMessage(object):
  """Message class contains information of a general stage failure.

  Failed stages report stage failures to CIDB failureTable (see more details
  in failures_lib.ReportStageFailureToCIDB). This class constructs a failure
  message instance from the stage failure information stored in CIDB.
  """

  def __init__(self, stage_failure, extra_info=None, stage_prefix_name=None):
    """Construct a StageFailureMessage instance.

    Args:
      stage_failure: An instance of StageFailure.
      extra_info: The extra info of the origin failure, default to None.
      stage_prefix_name: The prefix name (string) of the failed stage,
        default to None.
    """
    self.failure_id = stage_failure.id
    self.build_stage_id = stage_failure.build_stage_id
    self.stage_name = stage_failure.stage_name
    self.exception_type = stage_failure.exception_type
    self.exception_message = stage_failure.exception_message
    self.exception_category = stage_failure.exception_category
    self.outer_failure_id = stage_failure.outer_failure_id

    if extra_info is not None:
      self.extra_info = extra_info
    else:
      # No extra_info provided, decode extra_info from stage_failure.
      self.extra_info = self._DecodeExtraInfo(stage_failure.extra_info)

    if stage_prefix_name is not None:
      self.stage_prefix_name = stage_prefix_name
    else:
      # No stage_prefix_name provided, extra prefix name from stage_failure.
      self.stage_prefix_name = self._ExtractStagePrefixName(self.stage_name)

  def __str__(self):
    return ('[failure id] %s [stage name] %s [stage prefix name] %s '
            '[exception type] %s [exception category] %s [exception message] %s'
            ' [extra info] %s' %
            (self.failure_id, self.stage_name, self.stage_prefix_name,
             self.exception_type, self.exception_category,
             self.exception_message, self.extra_info))

  def _DecodeExtraInfo(self, extra_info):
    """Decode extra info json into dict.

    Args:
      extra_info: The extra_info of the origin exception, default to None.

    Returns:
      An empty dict if extra_info is None; extra_info itself if extra_info is
      a dict; else, load the json string into a dict and return it.
    """
    if not extra_info:
      return {}
    elif isinstance(extra_info, dict):
      return extra_info
    else:
      try:
        return  json.loads(extra_info)
      except ValueError as e:
        logging.error('Cannot decode extra_info: %s', e)
        return {}

  # TODO(nxia): Force format checking on stage names when they're created
  def _ExtractStagePrefixName(self, stage_name):
    """Extract stage prefix name given a full stage name.

    Format examples in our current CIDB buildStageTable:
      HWTest [bvt-arc] -> HWTest
      HWTest -> HWTest
      ImageTest -> ImageTest
      ImageTest [amd64-generic] -> ImageTest
      VMTest (attempt 1) -> VMTest
      VMTest [amd64-generic] (attempt 1) -> VMTest

    Args:
      stage_name: The full stage name (string) recorded in CIDB.

    Returns:
      The prefix stage name (string).
    """
    pattern = r'([^ ]+)( +\[([^]]+)\])?( +\(([^)]+)\))?'
    m = re.compile(pattern).match(stage_name)
    if m is not None:
      return m.group(1)
    else:
      return stage_name


class BuildScriptFailureMessage(StageFailureMessage):
  """Message class contains information of a BuildScriptFailure."""

  def __init__(self, stage_failure, **kwargs):
    """Construct a BuildScriptFailureMessage instance.

    Args:
      stage_failure: An instance of StageFailure.
      kwargs: Extra message information to pass to StageFailureMessage.
    """
    super(BuildScriptFailureMessage, self).__init__(stage_failure, **kwargs)

  def GetShortname(self):
    """Return the short name (string) of the run command."""
    return self.extra_info.get('shortname')


class PackageBuildFailureMessage(StageFailureMessage):
  """Message class contains information of a PackagebuildFailure."""

  def __init__(self, stage_failure, **kwargs):
    """Construct a PackageBuildFailureMessage instance.

    Args:
      stage_failure: An instance of StageFailure.
      kwargs: Extra message information to pass to StageFailureMessage.
    """
    super(PackageBuildFailureMessage, self).__init__(
        stage_failure, **kwargs)

  def GetShortname(self):
    """Return the short name (string) of the run command."""
    return self.extra_info.get('shortname')

  def GetFailedPackages(self):
    """Return a list of packages (strings) that failed to build."""
    return self.extra_info.get('failed_packages', [])


class CompoundFailureMessage(StageFailureMessage):
  """Message class contains information of a CompoundFailureMessage."""

  def __init__(self, stage_failure, **kwargs):
    """Construct a CompoundFailureMessage instance.

    Args:
      stage_failure: An instance of StageFailure.
      kwargs: Extra message information to pass to StageFailureMessage.
    """
    super(CompoundFailureMessage, self).__init__(stage_failure, **kwargs)

    self.inner_failures = []

  def __str__(self):
    msg_str = super(CompoundFailureMessage, self).__str__()

    for failure in self.inner_failures:
      msg_str += ('(Inner Stage Failure Message) %s' % str(failure))

    return msg_str

  @staticmethod
  def GetFailureMessage(failure_message):
    """Convert a regular failure message instance to CompoundFailureMessage.

    Args:
      failure_message: An instance of StageFailureMessage.

    Returns:
      A CompoundFailureMessage instance.
    """
    return CompoundFailureMessage(
        StageFailure.GetStageFailureFromMessage(failure_message),
        extra_info=failure_message.extra_info,
        stage_prefix_name=failure_message.stage_prefix_name)

  def HasEmptyList(self):
    """Check whether the inner failure list is empty.

    Returns:
      True if self.inner_failures is empty; else, False.
    """
    return not bool(self.inner_failures)

  def HasFailureType(self, exception_type):
    """Check whether any of the inner failures matches the exception type.

    Args:
      exception_type: The class name (string) of the origin exception.

    Returns:
      True if any of the inner failures matches exception_type; else, False.
    """
    return any(x.exception_type == exception_type for x in self.inner_failures)

  def MatchesFailureType(self, exception_type):
    """Check whether all of the inner failures match the exception type.

    Args:
      exception_type: The class name (string) of the origin exception.

    Returns:
      True if all of the inner failures match exception_type; else, False.
    """
    return (not self.HasEmptyList() and
            all(x.exception_type == exception_type
                for x in self.inner_failures))

  def HasExceptionCategory(self, exception_category):
    """Check whether any of the inner failures matches the exception category.

    Args:
      exception_category: The category of the origin exception (one of
        constants.EXCEPTION_CATEGORY_ALL_CATEGORIES).

    Returns:
      True if any of the inner failures matches exception_category; else, False.
    """
    return any(x.exception_category == exception_category
               for x in self.inner_failures)

  def MatchesExceptionCategory(self, exception_category):
    """Check whether all of the inner failures matches the exception category.

    Args:
      exception_category: The category of the origin exception (one of
        constants.EXCEPTION_CATEGORY_ALL_CATEGORIES).

    Returns:
      True if all of the inner failures match exception_category; else, False.
    """
    return (not self.HasEmptyList() and
            all(x.exception_category == exception_category
                for x in self.inner_failures))


class FailureMessageManager(object):
  """Manager class to create a failure message or reconstruct messages."""

  @classmethod
  def CreateMessage(cls, stage_failure, **kwargs):
    """Create a failure message instance depending on the exception type.

    Args:
      stage_failure: An instance of StageFailure.
      kwargs: Extra message information to pass to StageFailureMessage.

    Returns:
      A failure message instance of StageFailureMessage class (or its
        sub-class)
    """
    if stage_failure.exception_type in failures_lib.BUILD_SCRIPT_FAILURE_TYPES:
      return BuildScriptFailureMessage(stage_failure, **kwargs)
    elif (stage_failure.exception_type in
          failures_lib.PACKAGE_BUILD_FAILURE_TYPES):
      return PackageBuildFailureMessage(stage_failure, **kwargs)
    else:
      return StageFailureMessage(stage_failure, **kwargs)

  @classmethod
  def ReconstructMessages(cls, failure_messages):
    """Reconstruct failure messages by nesting messages.

    A failure message with not none outer_failure_id is an inner failure of its
    outer failure message(failure_id == outer_failure_id). This method takes a
    list of failure messages, reconstructs the list by 1) converting the outer
    failure message into a CompoundFailureMessage instance 2) insert the inner
    failure messages to the inner_failures list of their outer failure messages.
    CompoundFailures in CIDB aren't nested
    (see failures_lib.ReportStageFailureToCIDB), so there isn't another
    inner failure list layer in a inner failure message and there're no circular
    dependencies.

    For example, given failure_messages list
      [A(failure_id=1),
       B(failure_id=2, outer_failure_id=1),
       C(failure_id=3, outer_failure_id=1),
       D(failure_id=4),
       E(failure_id=5, outer_failure_id=4),
       F(failure_id=6)]
    this method returns a reconstructed list:
      [A(failure_id=1, inner_failures=[B(failure_id=2, outer_failure_id=1),
                                       C(failure_id=3, outer_failure_id=1)]),
       D(failure_id=4, inner_failures=[E(failure_id=5, outer_failure_id=4)]),
       F(failure_id=6)]

    Args:
      failure_messages: A list a failure message instances not nested.

    Returns:
      A list of failure message instances of StageFailureMessage class (or its
        sub-class). Failure messages with not None outer_failure_id are nested
        into the inner_failures list of their outer failure messages.
    """
    failure_message_dict = {x.failure_id: x for x in failure_messages}

    for failure in failure_messages:
      if failure.outer_failure_id is not None:
        assert failure.outer_failure_id in failure_message_dict
        outer_failure = failure_message_dict[failure.outer_failure_id]
        if not isinstance(outer_failure, CompoundFailureMessage):
          outer_failure = CompoundFailureMessage.GetFailureMessage(
              outer_failure)
          failure_message_dict[outer_failure.failure_id] = outer_failure

        outer_failure.inner_failures.append(failure)
        del failure_message_dict[failure.failure_id]

    return failure_message_dict.values()

  @classmethod
  def ConstructStageFailureMessages(cls, stage_failures):
    """Construct stage failure messages from failure entries from CIDB.

    Args:
      stage_failures: A list of StageFailure instances.

    Returns:
      A list of stage failure message instances of StageFailureMessage class
      (or its sub-class). See return type of ReconstructMessages().
    """
    failure_messages = [cls.CreateMessage(f) for f in stage_failures]

    return cls.ReconstructMessages(failure_messages)


class BuildFailureMessage(object):
  """Message indicating that changes failed to be validated.

  A failure message for a failed build, which is used to trige failures and
  detect bad changes.
  """

  def __init__(self, message_summary, failure_messages, internal, reason,
               builder):
    """Create a BuildFailureMessage instance.

    Args:
      message_summary: The message summary string to print.
      failure_messages: A list of failure messages (instances of
        StageFailureMessage), if any.
      internal: Whether this failure occurred on an internal builder.
      reason: A string describing the failure.
      builder: The builder the failure occurred on.
    """
    self.message_summary = str(message_summary)
    self.failure_messages = failure_messages or []
    self.internal = bool(internal)
    self.reason = str(reason)
    # builder should match build_config, e.g. self._run.config.name.
    self.builder = str(builder)

  def __str__(self):
    return self.message_summary

  def BuildFailureMessageToStr(self):
    """Return a string presenting the information in the BuildFailureMessage."""
    to_str = ('[builder] %s [message summary] %s [reason] %s [internal] %s\n' %
              (self.builder, self.message_summary, self.reason, self.internal))
    for f in self.failure_messages:
      to_str += '[failure message] ' + str(f) + '\n'

    return to_str

  def GetFailingStages(self):
    """Get a list of the failing stage prefixes from failure_messages.

    Returns:
      A list of failing stage prefixes if there are failure_messages; None
      otherwise.
    """
    failing_stages = None
    if self.failure_messages:
      failing_stages = set(x.stage_prefix_name for x in self.failure_messages)
    return failing_stages

  def MatchesExceptionCategory(self, exception_category):
    """Check if all of the failure_messages match the exception_category.

    Args:
      exception_category: The category of the origin exception (one of
        constants.EXCEPTION_CATEGORY_ALL_CATEGORIES).

    Returns:
      True if all of the failure_messages match the exception_category; else,
      False.
    """
    for failure in self.failure_messages:
      if failure.exception_category != exception_category:
        if (isinstance(failure, CompoundFailureMessage) and
            failure.MatchesExceptionCategory(exception_category)):
          continue
        else:
          return False

    return True

  def HasExceptionCategory(self, exception_category):
    """Check if any of the failure_messages match the exception_category.

    Args:
      exception_category: The category of the origin exception (one of
        constants.EXCEPTION_CATEGORY_ALL_CATEGORIES).

    Returns:
      True if any of the failure_messages match the exception_category; else,
      False.
    """
    for failure in self.failure_messages:
      if failure.exception_category == exception_category:
        return True

      if (isinstance(failure, CompoundFailureMessage) and
          failure.HasExceptionCategory(exception_category)):
        return True

    return False

  def MatchesFailureType(self, exception_type):
    """Check if all of the failure_messages match the exception_type.

    Args:
      exception_type: The class name (string) of the origin exception.

    Returns:
      True if all the failure_messages match the exception_type; else,
      False.
    """
    for failure in self.failure_messages:
      if failure.exception_type != exception_type:
        if (isinstance(failure, CompoundFailureMessage) and
            failure.MatchesFailureType(exception_type)):
          continue
        else:
          return False

    return True

  def HasFailureType(self, exception_type):
    """Check if any of the failure_messages match the exception_type.

    Args:
      exception_type: The class name (string) of the origin exception.

    Returns:
      True if any of the failure_messages match the exception_type; else,
      False.
    """
    for failure in self.failure_messages:
      if failure.exception_type == exception_type:
        return True

      if (isinstance(failure, CompoundFailureMessage) and
          failure.HasExceptionCategory(exception_type)):
        return True

    return False

  def IsPackageBuildFailure(self):
    """Check if all of the failures are package build failures."""
    return self.MatchesFailureType(failures_lib.PackageBuildFailure.__name__)

  def FindSuspectedChanges(self, changes, build_root, failed_hwtests, sanity):
    """Find and return suspected changes.

    Suspected changes are CLs that probably caused failures and will be
    rejected. This method analyzes every failure message and returns a set of
    changes as suspects.
    1) if a failure message is a PackageBuildFailure, get suspects for the build
    failure. If there're failed packages without assigned suspects, blame all
    changes when sanity is True.
    2) if a failure message is a TEST failure, get suspects for the HWTest
    failure. If there're failed HWTests without assigned suspects, blame all
    changes when sanity is True.
    3) If a failure message is neither PackagebuildFailure nor HWTestFailure,
    we can't explain the failure and so blame all changes when sanity is True.

    It is certainly possible to trick this algorithm: If one developer submits
    a change to libchromeos that breaks the power_manager, and another developer
    submits a change to the power_manager at the same time, only the
    power_manager change will be kicked out. That said, in that situation, the
    libchromeos change will likely be kicked out on the next run when the next
    run fails power_manager but dosen't include any changes from power_manager.

    Args:
      changes: A list of cros_patch.GerritPatch instances.
      build_root: The path to the build root.
      failed_hwtests: A list of name of failed hwtests got from CIDB (see the
        return type of HWTestResultManager.GetFailedHWTestsFromCIDB), or None.
      sanity: The sanity checker builder passed and the tree was open when
        the build started and ended.

    Returns:
      An instance of triage_lib.SuspectChanges.
    """
    suspect_changes = triage_lib.SuspectChanges()
    blame_everything = False
    for failure in self.failure_messages:
      if failure.exception_type == failures_lib.PackageBuildFailure.__name__:
        # Find suspects for PackageBuildFailure
        build_suspects, no_assignee_packages = (
            self.FindPackageBuildFailureSuspects(changes, failure))
        suspect_changes.update(
            {x: constants.SUSPECT_REASON_BUILD_FAIL for x in build_suspects})
        blame_everything = blame_everything or no_assignee_packages
      elif failure.exception_category == constants.EXCEPTION_CATEGORY_TEST:
        # Find suspects for HWTestFailure
        hwtest_suspects, no_assignee_hwtests = (
            hwtest_results.HWTestResultManager.FindHWTestFailureSuspects(
                changes, build_root, failed_hwtests))
        suspect_changes.update(
            {x: constants.SUSPECT_REASON_TEST_FAIL for x in hwtest_suspects})
        blame_everything = blame_everything or no_assignee_hwtests
      else:
        # Unknown failures, blame everything
        blame_everything = True

    # Only do broad-brush blaming if the tree is sane.
    if sanity:
      if blame_everything or len(suspect_changes) == 0:
        suspect_changes.update(
            {x: constants.SUSPECT_REASON_UNKNOWN for x in changes})
      else:
        # Never treat changes to overlays as innocent.
        overlay_changes = [x for x in changes if '/overlays/' in x.project]
        suspect_changes.update(
            {x: constants.SUSPECT_REASON_OVERLAY_CHANGE
             for x in overlay_changes})

    return suspect_changes

  def FindPackageBuildFailureSuspects(self, changes, failure):
    """Find suspects for a PackageBuild failure.

    If a change touched a package and that package broke, this change is one of
    the suspects; if multiple changes touched one failed package, all these
    changes will be returned as suspects.

    Args:
      changes: A list of cros_patch.GerritPatch instances.
      failure: An instance of StageFailureMessage(or its sub-class).

    Returns:
      A pair of suspects and no_assignee_packages. suspects is a set of
      cros_patch.GerritPatch instances as suspects. no_assignee_packages is True
      when there're failed packages without assigned suspects; else,
      no_assignee_packages is False.
    """
    suspects = set()
    no_assignee_packages = False
    packages_with_assignee = set()
    failed_packages = failure.GetFailedPackages()
    for package in failed_packages:
      failed_projects = portage_util.FindWorkonProjects([package])
      for change in changes:
        if change.project in failed_projects:
          suspects.add(change)
          packages_with_assignee.add(package)

    if suspects:
      logging.info('Find suspects for BuildPackages failures: %s',
                   cros_patch.GetChangesAsString(suspects))

    packages_without_assignee = set(failed_packages) - packages_with_assignee
    if packages_without_assignee:
      logging.info('Didn\'t find changes to blame for failed packages: %s',
                   list(packages_without_assignee))
      no_assignee_packages = True

    return suspects, no_assignee_packages
