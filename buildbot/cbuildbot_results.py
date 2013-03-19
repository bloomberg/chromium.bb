# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes for collecting results of our BuildStages as they run."""

import datetime
import math
import os

from chromite.lib import cros_build_lib


def _GetCheckpointFile(buildroot):
  return os.path.join(buildroot, '.completed_stages')


def WriteCheckpoint(buildroot):
  """Drops a completed stages file with current state."""
  completed_stages_file = _GetCheckpointFile(buildroot)
  with open(completed_stages_file, 'w+') as save_file:
    Results.SaveCompletedStages(save_file)


def LoadCheckpoint(buildroot):
  """Restore completed stage info from checkpoint file."""
  completed_stages_file = _GetCheckpointFile(buildroot)
  if not os.path.exists(completed_stages_file):
    cros_build_lib.Warning('Checkpoint file not found in buildroot %s'
                           % buildroot)
    return

  with open(completed_stages_file, 'r') as load_file:
    Results.RestoreCompletedStages(load_file)


class StepFailure(Exception):
  """StepFailure exceptions indicate that a cbuildbot step failed.

  Exceptions that derive from StepFailure should meet the following
  criteria:
    1) The failure indicates that a cbuildbot step failed.
    2) The necessary information to debug the problem has already been
       printed in the logs for the stage that failed.
    3) __str__() should be brief enough to include in a Commit Queue
       failure message.
  """


class BuildScriptFailure(StepFailure):
  """This exception is thrown when a build command failed.

  It is intended to provide a shorter summary of what command failed,
  for usage in failure messages from the Commit Queue, so as to ensure
  that developers aren't spammed with giant error messages when common
  commands (e.g. build_packages) fail.
  """

  def __init__(self, exception, shortname):
    """Construct a BuildScriptFailure object.

    Args:
      exception: A RunCommandError object.
      shortname: Short name for the command we're running.
    """
    StepFailure.__init__(self)
    assert isinstance(exception, cros_build_lib.RunCommandError)
    self.exception = exception
    self.shortname = shortname
    self.args = (exception, shortname)

  def __str__(self):
    """Summarize a build command failure briefly."""
    result = self.exception.result
    if result.returncode:
      return '%s failed (code=%s)' % (self.shortname, result.returncode)
    else:
      return self.exception.msg


class PackageBuildFailure(BuildScriptFailure):
  """This exception is thrown when packages fail to build."""

  def __init__(self, exception, shortname, failed_packages):
    """Construct a PackageBuildFailure object.

    Args:
      exception: The underlying exception.
      shortname: Short name for the command we're running.
      failed_packages: List of packages that failed to build.
    """
    BuildScriptFailure.__init__(self, exception, shortname)
    self.failed_packages = set(failed_packages)
    self.args += (failed_packages,)

  def __str__(self):
    return ('Packages failed in %s: %s'
            % (self.shortname, ' '.join(sorted(self.failed_packages))))


class RecordedTraceback(object):
  """This class represents a traceback recorded in the list of results."""

  def __init__(self, failed_stage, exception, traceback):
    """Construct a RecordedTraceback object.

    Args:
      failed_stage: The stage that failed during the build.
      exception: The raw exception object.
      traceback: The full stack trace for the failure, as a string.
    """
    self.failed_stage = failed_stage
    self.exception = exception
    self.traceback = traceback


class _Results(object):
  """Static class that collects the results of our BuildStages as they run."""

  SUCCESS = "Stage was successful"
  FORGIVEN = "Stage failed but was optional"
  SKIPPED = "Stage was skipped"
  NON_FAILURE_TYPES = (SUCCESS, FORGIVEN, SKIPPED)

  SPLIT_TOKEN = "\_O_/"

  def __init__(self):
    # List of results for all stages that's built up as we run. Members are of
    #  the form ('name', SUCCESS | FORGIVEN | Exception, None | description)
    self._results_log = []

    # Stages run in a previous run and restored. Stored as a dictionary of
    # names to previous records.
    self._previous = {}

  def Clear(self):
    """Clear existing stage results."""
    self.__init__()

  def PreviouslyCompletedRecord(self, name):
    """Check to see if this stage was previously completed.

       Returns:
         A boolean showing the stage was successful in the previous run.
    """
    return self._previous.get(name)

  def BuildSucceededSoFar(self):
    """Return true if all stages so far have passing states.

    This method returns true if all was successful or forgiven.
    """
    for entry in self._results_log:
      _, result, _, _ = entry
      if result not in self.NON_FAILURE_TYPES:
        return False

    return True

  def WasStageSuccessful(self, name):
    """Return true if stage passed."""
    cros_build_lib.Info('Checking for %s' % name)
    for entry in self._results_log:
      entry, result, _, _ = entry
      if entry == name:
        cros_build_lib.Info('Found %s' % result)
        return result == self.SUCCESS

    return False

  def StageHasResults(self, name):
    """Return true if stage has posted results."""
    return name in [entry[0] for entry in self._results_log]

  def Record(self, name, result, description=None, time=0):
    """Store off an additional stage result.

       Args:
         name: The name of the stage
         result:
           Result should be one of:
             Results.SUCCESS if the stage was successful.
             Results.SKIPPED if the stage was skipped.
             Results.FORGIVEN if the stage had warnings.
             Otherwise, it should be the exception stage errored with.
         description:
           The textual backtrace of the exception, or None
    """
    self._results_log.append((name, result, description, time))

  def UpdateResult(self, name, result, description=None):
    """Updates a stage result with a different result.

       Args:
         name: The name of the stage
         result: See docstring for Record above.
         description:
           The textual backtrace of the exception, or None
    """
    for index in range(len(self._results_log)):
      if self._results_log[index][0] == name:
        _, _, _, run_time = self._results_log[index]
        self._results_log[index] = name, result, description, run_time
        break

  def Get(self):
    """Fetch stage results.

       Returns:
         A list with one entry per stage run with a result.
    """
    return self._results_log

  def GetPrevious(self):
    """Fetch stage results.

       Returns:
         A list of stages names that were completed in a previous run.
    """
    return self._previous

  def SaveCompletedStages(self, out):
    """Save the successfully completed stages to the provided file |out|."""
    for name, result, description, time in self._results_log:
      if result != self.SUCCESS: break
      out.write(self.SPLIT_TOKEN.join([name, str(description), str(time)]))
      out.write('\n')

  def RestoreCompletedStages(self, out):
    """Load the successfully completed stages from the provided file |out|."""
    # Read the file, and strip off the newlines.
    for line in out:
      record = line.strip().split(self.SPLIT_TOKEN)
      if len(record) != 3:
        cros_build_lib.Warning(
            'State file does not match expected format, ignoring.')
        # Wipe any partial state.
        self._previous = {}
        break

      self._previous[record[0]] = record

  def GetTracebacks(self):
    """Get a list of the exceptions that failed the build.

    Returns:
       A list of RecordedTraceback objects.
    """
    for name, result, description, _ in self._results_log:
      # If result is not in NON_FAILURE_TYPES, then the stage failed, and
      # result is the exception object and description is a string containing
      # the full traceback.
      if result not in self.NON_FAILURE_TYPES:
        yield RecordedTraceback(name, result, description)

  def Report(self, out, archive_urls=None, current_version=None):
    """Generate a user friendly text display of the results data."""
    results = self._results_log

    line = '*' * 60 + '\n'
    edge = '*' * 2

    if current_version:
      out.write(line)
      out.write(edge +
                ' RELEASE VERSION: ' +
                current_version +
                '\n')

    out.write(line)
    out.write(edge + ' Stage Results\n')

    for name, result, _, run_time in results:
      timestr = datetime.timedelta(seconds=math.ceil(run_time))

      # Don't print data on skipped stages.
      if result == self.SKIPPED:
        continue

      out.write(line)
      details = ''
      if result == self.SUCCESS:
        status = 'PASS'
      elif result == self.FORGIVEN:
        status = 'FAILED BUT FORGIVEN'
      else:
        status = 'FAIL'
        if isinstance(result, cros_build_lib.RunCommandError):
          # If there was a RunCommand error, give just the command that
          # failed, not its full argument list, since those are usually
          # too long.
          details = ' in %s' % result.result.cmd[0]
        elif isinstance(result, BuildScriptFailure):
          # BuildScriptFailure errors publish a 'short' name of the
          # command that failed.
          details = ' in %s' % result.shortname
        else:
          # There was a normal error. Give the type of exception.
          details = ' with %s' % type(result).__name__

      out.write('%s %s %s (%s)%s\n' % (edge, status, name, timestr, details))

    out.write(line)

    if archive_urls:
      out.write('%s BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n' % edge)
      for board, url in sorted(archive_urls.iteritems()):
        out.write('%s  %s: %s' % (edge, board, url))
        cros_build_lib.PrintBuildbotLink('Artifacts[%s]' % board, url,
                                         handle=out)
      out.write(line)

    for x in self.GetTracebacks():
      if x.failed_stage and x.traceback:
        out.write('\nFailed in stage %s:\n\n' % x.failed_stage)
        out.write(x.traceback)
        out.write('\n')

Results = _Results()
