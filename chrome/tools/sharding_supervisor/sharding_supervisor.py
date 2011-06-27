#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Shards a given test suite and runs the shards in parallel.

ShardingSupervisor is called to process the command line options and creates
the specified number of worker threads. These threads then run each shard of
the test in a separate process and report on the results. When all the shards
have been completed, the supervisor reprints any lines indicating a test
failure for convenience. If only one shard is to be run, a single subprocess
is started for that shard and the output is identical to gtest's output.

Usage: python sharding_supervisor.py [options] path/to/test [gtest_args]
"""


import optparse
import os
import pty
import Queue
import subprocess
import sys
import threading


DEFAULT_NUM_CORES = 4
DEFAULT_SHARDS_PER_CORE = 5 # num_shards = cores * SHARDS_PER_CORE
DEFAULT_RUNS_PER_CORE = 1 # num_workers = cores * RUNS_PER_CORE


def DetectNumCores():
  """Detects the number of cores on the machine.

  Returns:
    The number of cores on the machine or DEFAULT_NUM_CORES if it could not
    be found.
  """
  try:
    # Linux, Unix, MacOS
    if hasattr(os, "sysconf"):
      if "SC_NPROCESSORS_ONLN" in os.sysconf_names:
        # Linux, Unix
        return int(os.sysconf("SC_NPROCESSORS_ONLN"))
      else:
        # OSX
        return int(os.popen2("sysctl -n hw.ncpu")[1].read())
    # Windows
    return int(os.environ["NUMBER_OF_PROCESSORS"])
  except ValueError:
    return DEFAULT_NUM_CORES


def RunShard(test, num_shards, index, gtest_args, stdout, stderr):
  """Runs a single test shard in a subprocess.

  Returns:
    The Popen object representing the subprocess handle.
  """
  args = [test]
  args.extend(gtest_args)
  env = os.environ.copy()
  env["GTEST_TOTAL_SHARDS"] = str(num_shards)
  env["GTEST_SHARD_INDEX"] = str(index)
  return subprocess.Popen(
      args, stdout=stdout, stderr=stderr, env=env)


class ShardRunner(threading.Thread):
  """Worker thread that manages a single shard at a time.

  Attributes:
    supervisor: The ShardingSupervisor that this worker reports to.
    counter: Called to get the next shard index to run.
  """

  def __init__(self, supervisor, counter):
    """Inits ShardRunner with a supervisor and counter."""
    threading.Thread.__init__(self)
    self.supervisor = supervisor
    self.counter = counter

  def run(self):
    """Runs shards and outputs the results.

    Gets the next shard index from the supervisor, runs it in a subprocess,
    and collects the output. Each line is prefixed with 'N>', where N is the
    current shard index.
    """
    while True:
      try:
        index = self.counter.get_nowait()
      except Queue.Empty:
        break
      shard = RunShard(
          self.supervisor.test, self.supervisor.num_shards, index,
          self.supervisor.gtest_args, subprocess.PIPE, subprocess.STDOUT)
      while True:
        line = shard.stdout.readline()
        if not line:
          if shard.poll() is not None:
            break
          continue
        line = "%i>%s" % (index, line)
        if (line.find("FAIL", 0, 20) >= 0 and line.find(".") >= 0 and
            line.find("ms)")) < 0:
          self.supervisor.LogLineFailure(line)
        sys.stdout.write(line)
      if shard.returncode != 0:
        self.supervisor.LogShardFailure(index)


class ShardingSupervisor(object):
  """Supervisor object that handles the worker threads.

  Attributes:
    test: Name of the test to shard.
    num_shards: Total number of shards to split the test into.
    num_runs: Total number of worker threads to create for running shards.
    color: Indicates which coloring mode to use in the output.
    gtest_args: The options to pass to gtest.
    failure_log: List of statements from shard output indicating a failure.
    failed_shards: List of shards that contained failing tests.
  """

  def __init__(
      self, test, num_shards, num_runs, color, gtest_args):
    """Inits ShardingSupervisor with given options and gtest arguments."""
    self.test = test
    self.num_shards = num_shards
    self.num_runs = num_runs
    self.color = color
    self.gtest_args = gtest_args
    self.failure_log = []
    self.failed_shards = []

  def ShardTest(self):
    """Runs the test and manages the worker threads.

    Runs the test and outputs a summary at the end. All the tests in the
    suite are run by creating (cores * runs_per_core) threads and
    (cores * shards_per_core) shards. When all the worker threads have
    finished, the lines saved in the failure_log are printed again.

    Returns:
      The number of shards that had failing tests.
    """
    workers = []
    counter = Queue.Queue()
    for i in range(self.num_shards):
      counter.put(i)
    for i in range(self.num_runs):
      worker = ShardRunner(self, counter)
      worker.start()
      workers.append(worker)
    for worker in workers:
      worker.join()
    return self.PrintSummary()

  def LogLineFailure(self, line):
    """Saves a line in the failure log to be printed at the end.

    Args:
      line: The line to save in the failure_log.
    """
    self.failure_log.append(line)

  def LogShardFailure(self, index):
    """Records that a test in the given shard has failed.

    Args:
      index: The index of the failing shard.
    """
    self.failed_shards.append(index)

  def PrintSummary(self):
    """Prints a summary of the test results.

    If any shards had failing tests, the list is sorted and printed. Then all
    the lines that indicate a test failure are reproduced.

    Returns:
      The number of shards that had failing tests.
    """
    sys.stderr.write("\n")
    num_failed = len(self.failed_shards)
    if num_failed > 0:
      self.failed_shards.sort()
      if self.color:
        sys.stderr.write("\x1b[1;5;31m")
      sys.stderr.write("FAILED SHARDS: %s\n" % str(self.failed_shards))
    else:
      if self.color:
        sys.stderr.write("\x1b[1;5;32m")
      sys.stderr.write("ALL SHARDS PASSED!\n")
    if self.failure_log:
      if self.color:
        sys.stderr.write("\x1b[1;5;31m")
      sys.stderr.write("FAILED TESTS:\n")
      if self.color:
        sys.stderr.write("\x1b[0;37m")
      for line in self.failure_log:
        sys.stderr.write(line)
    if self.color:
      sys.stderr.write("\x1b[0;37m")
    return num_failed


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      "-n", "--shards_per_core", type="int", default=DEFAULT_SHARDS_PER_CORE,
      help="number of shards to generate per CPU")
  parser.add_option(
      "-r", "--runs_per_core", type="int", default=DEFAULT_RUNS_PER_CORE,
      help="number of shards to run in parallel per CPU")
  parser.add_option(
      "-c", "--color", action="store_true", default=sys.stdout.isatty(),
      help="force color output, also used by gtest if --gtest_color is not"
      " specified")
  parser.add_option(
      "--no-color", action="store_false", dest="color",
      help="disable color output")
  parser.add_option(
      "-s", "--runshard", type="int", help="single shard index to run")
  parser.disable_interspersed_args()
  (options, args) = parser.parse_args()

  if not args:
    parser.error("You must specify a path to test!")
  if not os.path.exists(args[0]):
    parser.error("%s does not exist!" % args[0])

  num_cores = DetectNumCores()

  if options.shards_per_core < 1:
    parser.error("You must have at least 1 shard per core!")
  num_shards = num_cores * options.shards_per_core

  if options.runs_per_core < 1:
    parser.error("You must have at least 1 run per core!")
  num_runs = num_cores * options.runs_per_core

  gtest_args = ["--gtest_color=%s" % {
      True: "yes", False: "no"}[options.color]] + args[1:]

  if options.runshard != None:
    # run a single shard and exit
    if (options.runshard < 0 or options.runshard >= num_shards):
      parser.error("Invalid shard number given parameters!")
    shard = RunShard(
        args[0], num_shards, options.runshard, gtest_args, None, None)
    shard.communicate()
    return shard.poll()

  # shard and run the whole test
  ss = ShardingSupervisor(
      args[0], num_shards, num_runs, options.color, gtest_args)
  return ss.ShardTest()


if __name__ == "__main__":
  sys.exit(main())

