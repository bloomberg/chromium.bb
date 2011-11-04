# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes for collecting results of our BuildStages as they run."""

import datetime
import math

from chromite.lib import cros_build_lib as cros_lib

class Results:
  """Static class that collects the results of our BuildStages as they run."""

  # List of results for all stages that's built up as we run. Members are of
  #  the form ('name', SUCCESS | SKIPPED | Exception, None | description)
  _results_log = []

  # Stages run in a previous run and restored. Stored as a list of
  # stage names.
  _previous = []

  # Stored in the results log for a stage skipped because it was previously
  # completed successfully.
  SUCCESS = "Stage was successful"
  SKIPPED = "Stage skipped because successful on previous run"
  FORGIVEN = "Stage failed but was optional"

  @classmethod
  def Clear(cls):
    """Clear existing stage results."""
    cls._results_log = []
    cls._previous = []

  @classmethod
  def PreviouslyCompleted(cls, name):
    """Check to see if this stage was previously completed.

       Returns:
         A boolean showing the stage was successful in the previous run.
    """
    return name in cls._previous

  @classmethod
  def Success(cls):
    """Return true if all stages so far have passed.

    This method returns true if all was successful or forgiven.
    """
    for entry in cls._results_log:
      _, result, _, _ = entry

      if not result in (cls.SUCCESS, cls.SKIPPED, cls.FORGIVEN):
        return False

    return True

  @classmethod
  def WasStageSuccessfulOrSkipped(cls, name):
    """Return true stage passed."""
    for entry in cls._results_log:
      entry, result, _, _ = entry

      if entry == name:
        if result not in (cls.SUCCESS, cls.SKIPPED):
          return False
        else:
          return True

    return False

  @classmethod
  def Record(cls, name, result, description=None, time=0):
    """Store off an additional stage result.

       Args:
         name: The name of the stage
         result:
           Result should be one of:
             Results.SUCCESS if the stage was successful.
             Results.SKIPPED if the stage was completed in a previous run.
             The exception the stage errored with.
         description:
           The textual backtrace of the exception, or None
    """
    cls._results_log.append((name, result, description, time))

  @classmethod
  def UpdateResult(cls, name, result, description=None):
    """Updates a stage result.with a different result.

       Args:
         name: The name of the stage
         result:
           Result should be one of:
             Results.SUCCESS if the stage was successful.
             Results.SKIPPED if the stage was completed in a previous run.
             The exception the stage errored with.
          description:
           The textual backtrace of the exception, or None
    """
    for index in range(len(cls._results_log)):
      if cls._results_log[index][0] == name: break
    else:
      return

    _, _, _, run_time = cls._results_log[index]
    cls._results_log[index] = name, result, description, run_time

  @classmethod
  def Get(cls):
    """Fetch stage results.

       Returns:
         A list with one entry per stage run with a result.
    """
    return cls._results_log

  @classmethod
  def GetPrevious(cls):
    """Fetch stage results.

       Returns:
         A list of stages names that were completed in a previous run.
    """
    return cls._previous

  @classmethod
  def SaveCompletedStages(cls, file):
    """Save out the successfully completed stages to the provided file."""
    for name, result, _, _ in cls._results_log:
      if result != cls.SUCCESS and result != cls.SKIPPED: break
      file.write(name)
      file.write('\n')

  @classmethod
  def RestoreCompletedStages(cls, file):
    """Load the successfully completed stages from the provided file."""
    # Read the file, and strip off the newlines
    cls._previous = [line.strip() for line in file.readlines()]

  @classmethod
  def Report(cls, file, archive_url=None, current_version=None):
    """Generate a user friendly text display of the results data."""
    results = cls._results_log

    line = '*' * 60 + '\n'
    edge = '*' * 2

    if current_version:
      file.write(line)
      file.write(edge +
                 ' RELEASE VERSION: ' +
                 current_version +
                 '\n')

    file.write(line)
    file.write(edge + ' Stage Results\n')

    first_exception = None

    for name, result, description, run_time in results:
      timestr = datetime.timedelta(seconds=math.ceil(run_time))

      file.write(line)

      if result == cls.SUCCESS:
        # These was no error
        file.write('%s PASS %s (%s)\n' % (edge, name, timestr))

      elif result == cls.SKIPPED:
        # The stage was executed previously, and skipped this time
        file.write('%s %s previously completed or did not need to run.\n' %
                   (edge, name))
      elif result == cls.FORGIVEN:
        # The stage was executed previously, and skipped this time
        file.write('%s FAILED BUT FORGIVEN %s (%s)\n' %
                   (edge, name, timestr))
      else:
        if type(result) in (cros_lib.RunCommandException,
                            cros_lib.RunCommandError):
          # If there was a RunCommand error, give just the command that
          # failed, not it's full argument list, since those are usually
          # too long.
          file.write('%s FAIL %s (%s) in %s\n' %
                     (edge, name, timestr, result.cmd[0]))
        else:
          # There was a normal error, give the type of exception
          file.write('%s FAIL %s (%s) with %s\n' %
                     (edge, name, timestr, type(result).__name__))

        if not first_exception:
          first_exception = description

    file.write(line)

    if archive_url:
      file.write('%s BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n' % edge)
      file.write('%s  %s\n' % (edge, archive_url))
      file.write('@@@STEP_LINK@Artifacts@%s@@@\n' % archive_url)
      file.write(line)

    if first_exception:
      file.write('\n')
      file.write('Build failed with:\n')
      file.write('\n')
      file.write(first_exception)
      file.write('\n')
