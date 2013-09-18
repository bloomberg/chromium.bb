# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import chrome_helper


def VerifyProcesses(processes, variable_expander):
  """Verifies that the running processes match the expectation dictionaries.

  This method will throw an AssertionError if process state doesn't match the
  provided expectation.

  Args:
    processes: A dictionary whose keys are paths to processes and values are
        expectation dictionaries. An expectation dictionary is a dictionary with
        the following key and value:
            'running' a boolean indicating whether the process should be
                running.
    variable_expander: A VariableExpander object.
  """
  # Create a list of paths of all running processes.
  running_process_paths = [path for (_, path) in
                           chrome_helper.GetProcessIDAndPathPairs()]

  for process_path, expectation in processes.iteritems():
    process_expanded_path = variable_expander.Expand(process_path)
    is_running = process_expanded_path in running_process_paths
    assert expectation['running'] == is_running, \
        ('Process %s is running' % process_expanded_path) if is_running else \
        ('Process %s is not running' % process_expanded_path)
