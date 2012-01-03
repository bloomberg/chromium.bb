# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes for collecting results of our BuildStages as they run."""

import datetime
import math

from chromite.lib import cros_build_lib as cros_lib

class _Results(object):
  """Static class that collects the results of our BuildStages as they run."""

  # Stored in the results log for a stage skipped because it was previously
  # completed successfully.
  SUCCESS = "Stage was successful"
  FORGIVEN = "Stage failed but was optional"
  SPLIT_TOKEN = "\_O_/"

  def __init__(self):
    self.Clear()

  def Clear(self):
    """Clear existing stage results."""

    # List of results for all stages that's built up as we run. Members are of
    #  the form ('name', SUCCESS | FORGIVEN | Exception, None | description)
    self._results_log = []

    # Stages run in a previous run and restored. Stored as a dictionary of
    # names to previous records.
    self._previous = {}

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
      if not result in (self.SUCCESS, self.FORGIVEN):
        return False

    return True

  def WasStageSuccessful(self, name):
    """Return true stage passed."""
    cros_lib.Info('Checking for %s' % name)
    for entry in self._results_log:
      entry, result, _, _ = entry
      if entry == name:
        cros_lib.Info('Found %s' % result)
        return result == self.SUCCESS

    return False

  def Record(self, name, result, description=None, time=0):
    """Store off an additional stage result.

       Args:
         name: The name of the stage
         result:
           Result should be one of:
             Results.SUCCESS if the stage was successful.
             The exception the stage errored with.
         description:
           The textual backtrace of the exception, or None
    """
    self._results_log.append((name, result, description, time))

  def UpdateResult(self, name, result, description=None):
    """Updates a stage result with a different result.

       Args:
         name: The name of the stage
         result:
           Result should be Results.SUCCESS if the stage was successful
             otherwise the exception the stage errored with.
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
        cros_lib.Warning('State file does not match expected format, ignoring.')
        # Wipe any partial state.
        self._previous = {}
        break

      self._previous[record[0]] = record


  def Report(self, out, archive_url=None, current_version=None):
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

    first_exception = None

    for name, result, description, run_time in results:
      timestr = datetime.timedelta(seconds=math.ceil(run_time))

      out.write(line)
      if result == self.SUCCESS:
        # These was no error
        out.write('%s PASS %s (%s)\n' % (edge, name, timestr))

      elif result == self.FORGIVEN:
        # The stage was executed previously, and skipped this time
        out.write('%s FAILED BUT FORGIVEN %s (%s)\n' %
                   (edge, name, timestr))
      else:
        if type(result) in (cros_lib.RunCommandException,
                            cros_lib.RunCommandError):
          # If there was a RunCommand error, give just the command that
          # failed, not it's full argument list, since those are usually
          # too long.
          out.write('%s FAIL %s (%s) in %s\n' %
                     (edge, name, timestr, result.cmd[0]))
        else:
          # There was a normal error, give the type of exception
          out.write('%s FAIL %s (%s) with %s\n' %
                     (edge, name, timestr, type(result).__name__))

        if not first_exception:
          first_exception = description

    out.write(line)

    if archive_url:
      out.write('%s BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n' % edge)
      out.write('%s  %s\n' % (edge, archive_url))
      out.write('@@@STEP_LINK@Artifacts@%s@@@\n' % archive_url)
      out.write(line)

    if first_exception:
      out.write('\n')
      out.write('Build failed with:\n')
      out.write('\n')
      out.write(first_exception)
      out.write('\n')

Results = _Results()
