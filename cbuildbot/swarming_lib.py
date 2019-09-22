# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for running commands via swarming instance."""

from __future__ import print_function

import itertools
import json
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import retry_util
from chromite.lib import timeout_util

# Location of swarming_client.py that is used to send swarming requests
_DIR_NAME = os.path.dirname(os.path.abspath(__file__))
_SWARMING_PROXY_CLIENT = os.path.abspath(os.path.join(
    _DIR_NAME, '..', 'third_party', 'swarming.client', 'swarming.py'))
CONNECTION_TYPE_COMMON = 'common'
CONNECTION_TYPE_MOCK = 'mock'
# Code 80 - bot died.
RETRIABLE_INTERNAL_FAILURE_STATES = {80}
# Choose a prime number here to make the lowest common multiple of suite
# timeout_mins and silence check interval large enough.
SILENCE_INTERVAL_MIN = 97


def RunSwarmingCommand(cmd, swarming_server, is_skylab=False,
                       task_name=None, dimensions=None,
                       priority=None,
                       print_status_updates=False,
                       timeout_secs=None, io_timeout_secs=None,
                       hard_timeout_secs=None, expiration_secs=None,
                       temp_json_path=None, tags=None,
                       service_account_json=None,
                       *args, **kwargs):
  """Run command via swarming proxy.

  Args:
    cmd: Commands to run, represented as a list.
    swarming_server: The swarming server to send request to.
    is_skylab: A boolean indicating whether the call is for skylab.
    task_name: String, represent a task.
    dimensions: A list of tuple with two elements, representing dimension for
               selecting a swarming bots. E.g. ('os', 'Linux') and pools and
               other dimension related stuff.
    priority: An int number to indicate the priority of this swarming cmd.
        By default it's None.
    print_status_updates: Boolean, whether to output status updates,
                          can be used to prevent from hitting
                          buildbot silent timeout.
    timeout_secs: Timeout to wait for result used by swarming client.
    hard_timeout_secs: Seconds to allow the task to complete.
    io_timeout_secs: Seconds to allow the task to be silent.
    expiration_secs: Seconds to allow the task to be pending for a bot to
                     run before this task request expires.
    temp_json_path: Where swarming client should dump the result.
    tags: Dict, representing tags to add to the swarming command.
    service_account_json: Location of the service account json file.
  """
  with osutils.TempDir() as tempdir:
    if temp_json_path is None:
      temp_json_path = os.path.join(tempdir, 'temp_summary.json')

    swarming_cmd = [_SWARMING_PROXY_CLIENT, 'run', '--swarming',
                    swarming_server]
    swarming_cmd += ['--task-summary-json', temp_json_path]
    if print_status_updates:
      swarming_cmd.append('--print-status-updates')

    if timeout_secs is not None:
      swarming_cmd += ['--timeout', str(timeout_secs)]

    swarming_cmd += ['--raw-cmd']

    if task_name:
      swarming_cmd += ['--task-name', task_name]

    if priority is not None:
      swarming_cmd += ['--priority', str(priority)]

    if dimensions:
      for dimension in dimensions:
        swarming_cmd += ['--dimension', dimension[0], dimension[1]]

    if io_timeout_secs is not None:
      swarming_cmd += ['--io-timeout', str(io_timeout_secs)]

    if hard_timeout_secs is not None:
      swarming_cmd += ['--hard-timeout', str(hard_timeout_secs)]

    if expiration_secs is not None:
      swarming_cmd += ['--expiration', str(expiration_secs)]

    if tags is not None:
      for k, v in tags.items():
        swarming_cmd += ['--tags=%s:%s' % (k, v)]

    if service_account_json:
      swarming_cmd += ['--auth-service-account-json', service_account_json]

    swarming_cmd += ['--']
    swarming_cmd += cmd

    # Ensure SWARMING_TASK_ID is unset in env. See crbug.com/821227.
    kwargs.setdefault('env', os.environ.copy())
    kwargs['env'].pop('SWARMING_TASK_ID', None)

    try:
      result = None
      for iteration in itertools.count(0):
        try:
          # Add a timeout limit of SILENCE_INTERVAL_MIN mins here to avoid
          # buildbot salency check.
          with timeout_util.Timeout(SILENCE_INTERVAL_MIN * 60):
            logging.info('Re-run swarming_cmd to avoid buildbot salency check.')
            if is_skylab:
              result = cros_build_lib.run(
                  swarming_cmd + ['--passed_mins',
                                  str(iteration * SILENCE_INTERVAL_MIN)],
                  *args, **kwargs)
            else:
              result = cros_build_lib.run(swarming_cmd, *args, **kwargs)
            break
        except timeout_util.TimeoutError:
          pass

      return SwarmingCommandResult.CreateSwarmingCommandResult(
          task_summary_json_path=temp_json_path, command_result=result)
    except cros_build_lib.RunCommandError as e:
      result = SwarmingCommandResult.CreateSwarmingCommandResult(
          task_summary_json_path=temp_json_path, command_result=e.result)
      raise cros_build_lib.RunCommandError(e.msg, result, e.exception)


def SwarmingRetriableErrorCheck(exception):
  """Check if a swarming error is retriable.

  Args:
    exception: A cros_build_lib.RunCommandError exception.

  Returns:
    True if retriable, otherwise False.
  """
  if not isinstance(exception, cros_build_lib.RunCommandError):
    logging.warning('Exception is not retriable: %s', str(exception))
    return False
  result = exception.result
  if not isinstance(result, SwarmingCommandResult):
    logging.warning('Exception is not retriable as the result '
                    'is not a SwarmingCommandResult: %s', str(result))
    return False
  if result.task_summary_json:
    try:
      internal_failure = result.GetValue('internal_failure')
      state = result.GetValue('state')
      if internal_failure and state in RETRIABLE_INTERNAL_FAILURE_STATES:
        logging.warning(
            'Encountered retriable swarming internal failure: %s',
            json.dumps(result.task_summary_json, indent=2))
        return True
    except (IndexError, KeyError) as e:
      logging.warning(
          'Could not determine if exception is retriable. Exception: %s. '
          'Error: %s. Swarming summary json: %s',
          str(exception), str(e),
          json.dumps(result.task_summary_json, indent=2))
      return False

  logging.warning('Exception is not retriable %s', str(exception))
  return False


def RunSwarmingCommandWithRetries(max_retry, *args, **kwargs):
  """Wrapper for RunSwarmingCommand that will retry a command.

  Args:
    max_retry: See RetryCommand.
    *args: See RetryCommand and RunSwarmingCommand.
    **kwargs: See RetryCommand and RunSwarmingCommand.

  Returns:
    A SwarmingCommandResult object.

  Raises:
    RunCommandError: When the command fails.
  """
  return retry_util.RetryCommand(RunSwarmingCommand, max_retry, *args, **kwargs)


class SwarmingCommandResult(cros_build_lib.CommandResult):
  """An object to store result of a command that is run via swarming."""

  def __init__(self, task_summary_json, *args, **kwargs):
    """Initialize.

    Args:
      task_summary_json: A dictionary, loaded from the json file output by
          swarming client. It cantains all details about the swarming task.
    """
    super(SwarmingCommandResult, self).__init__(*args, **kwargs)
    self.task_summary_json = task_summary_json

  @staticmethod
  def LoadJsonSummary(task_summary_json_path):
    """Load json file into a dict.

    Args:
      task_summary_json_path: A json that contains output of a swarming task.

    Returns:
      A dictionary or None if task_summary_json_path doesn't exist.
    """
    if os.path.exists(task_summary_json_path):
      logging.debug('Loading summary json file: %s', task_summary_json_path)
      with open(task_summary_json_path) as f:
        return json.load(f)

  @staticmethod
  def CreateSwarmingCommandResult(task_summary_json_path, command_result):
    """Create a SwarmingCommandResult object from a CommandResult object.

    Args:
      task_summary_json_path: The path to a json file that contains
                              output of a swarming task.
      command_result: A CommandResult object.

    Returns:
      A SwarmingCommandResult object.
    """
    task_summary_json = SwarmingCommandResult.LoadJsonSummary(
        task_summary_json_path)
    return  SwarmingCommandResult(task_summary_json=task_summary_json,
                                  cmd=command_result.cmd,
                                  error=command_result.error,
                                  output=command_result.output,
                                  returncode=command_result.returncode)

  def HasValidSummary(self):
    """Check whether the result has valid summary json.

    Returns:
      True if the summary is valid else False.
    """
    if not self.task_summary_json:
      logging.warning('Failed to load task summary json')
      return False

    if 'shards' not in self.task_summary_json:
      logging.error('No shards in the invalid task summary json file:\n%r',
                    self.task_summary_json)
      return False

    try:
      return self.task_summary_json.get('shards')[0]
    except TypeError as e:
      logging.error('Invalid content in task summary json file:%s\n%r', str(e),
                    self.task_summary_json)
      return False


  def GetValue(self, field, default=None):
    """Get the value of |field| from the json summary.

    Args:
      field: Name of the field.
      default: Default value if field does not exist.

    Returns:
      Value of the field.
    """
    if self.HasValidSummary():
      # Hack for crbug.com/951373, will be changed after CL:1159239 is merged.
      if (field == 'outputs' and
          field not in self.task_summary_json.get('shards')[0]):
        res = self.task_summary_json.get('shards')[0].get('output', default)
        return [res]

      return self.task_summary_json.get('shards')[0].get(field, default)
    return default
