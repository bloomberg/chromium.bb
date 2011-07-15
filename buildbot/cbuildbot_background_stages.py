# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for running cbuildbot stages in the background."""

import multiprocessing
import os
import sys
import tempfile

from chromite.buildbot import cbuildbot_stages as stages

def _PrintFile(filename):
  """Print the contents of the specified file to stdout."""
  output = file(filename)
  for line in output:
    sys.stdout.write(line)

  output.close()


class BackgroundStages(multiprocessing.Process):
  """Run a list of stages in sequence in the background."""

  def __init__(self):
    multiprocessing.Process.__init__(self)
    self._stages = []
    self._queue = multiprocessing.Queue()

  def AddStage(self, stage):
    """Add a stage to the list of stages to run in the background."""
    self._stages.append(stage)

  def WaitForStage(self):
    """Wait for the first stage to complete.

    Return whether an exception was caught."""
    assert not self.Empty()
    error, results, filename = self._queue.get()
    _PrintFile(filename)
    os.unlink(filename)
    self._stages.pop(0)
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
    for stage in self._stages:
      # Send all output to a named temporary file.
      output = tempfile.NamedTemporaryFile(delete=False)
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
      self._queue.put((error, results, output.name))
