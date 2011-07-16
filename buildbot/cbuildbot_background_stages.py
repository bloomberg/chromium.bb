# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for running cbuildbot stages in the background."""

import multiprocessing
import os
import Queue
import sys
import tempfile

from chromite.buildbot import cbuildbot_stages as stages

_PRINT_INTERVAL = 1

class BackgroundStages(multiprocessing.Process):
  """Run a list of stages in sequence in the background."""

  def __init__(self):
    multiprocessing.Process.__init__(self)
    self._stages = []
    self._files = []
    self._queue = multiprocessing.Queue()

  def AddStage(self, stage):
    """Add a stage to the list of stages to run in the background."""
    output = tempfile.NamedTemporaryFile(delete=False)
    self._stages.append((stage, output))

  def WaitForStage(self):
    """Wait for the first stage to complete.

    Return whether an exception was caught."""
    assert not self.Empty()
    stage, output = self._stages.pop(0)
    pos = 0
    more_output = True
    while more_output:
      # Check whether the process is finished.
      try:
        error, results = self._queue.get(True, _PRINT_INTERVAL)
        more_output = False
      except Queue.Empty:
        more_output = True

      # Print output so far.
      output.seek(pos)
      for line in output:
        sys.stdout.write(line)
      pos = output.tell()

    output.close()
    os.unlink(output.name)
    for result in results:
      stages.Results.Record(*result)

    return error

  def Empty(self):
    """Return True if there are any stages left to run."""
    return len(self._stages) == 0

  def run(self):
    """Run the list of stages."""

    # Be nice so that foreground processes get CPU if they need it.
    os.nice(10)

    stdout_fileno = sys.stdout.fileno()
    stderr_fileno = sys.stderr.fileno()
    for stage, output in self._stages:
      # Send all output to a named temporary file.
      os.dup2(output.fileno(), stdout_fileno)
      os.dup2(output.fileno(), stderr_fileno)
      error = True
      try:
        stages.Results.Clear()
        stage.Run()
        error = False
      except:
        traceback.print_exc(file=output)

      output.close()
      results = stages.Results.Get()
      self._queue.put((error, results))
