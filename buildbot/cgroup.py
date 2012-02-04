#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class for managing the Linux cgroup subsystem."""

import os
import sys
import time

import constants
sys.path.insert(0, constants.SOURCE_ROOT)

from chromite.lib import cros_build_lib as cros_lib


class CGroup(object):
  PROC_PATH = '/proc/cgroups'
  MOUNTS_PATH = '/proc/mounts'
  CGROUP_ROOT = '/sys/fs/cgroup'

  # Kernel subsystems that we request for the hierarchy.
  # Note: It is needed to have at least one subsystem. Selection varies
  # based on kernel configuration, many are oddly broken and cannot be
  # reliably used just yet.
  NEEDED_SUBSYSTEMS = ('cpuset',)

  @property
  def cgroup_supported(self):
    val = getattr(self, '_cgroup_supported', None)
    if val is None:
      val = self.CheckCGroupSupport()
      self._cgroup_supported = val
    return val

  def FileContains(self, filename, strings):
    """Greps a group of expressions, returns whether all were found."""
    with open(filename, 'r') as f:
      contents = f.read()
      for s in strings:
        if not s in contents:
          return False

    return True

  def CheckCGroupSupport(self):
    """Checks cgroup support in the running system."""
    # Is the cgroup subsystem even enabled?
    if not os.path.exists(self.PROC_PATH):
      return False

    # Does it support the subsystems we want?
    if not self.FileContains(self.PROC_PATH, self.NEEDED_SUBSYSTEMS):
      return False

    # For older kernels, this did not exist.
    if not os.path.exists(self.CGROUP_ROOT):
      self.CGROUP_ROOT = '/dev/cgroup'
      # Unlike /sys/fs/cgroup, this path is not autocreated.
      if not os.path.exists(self.CGROUP_ROOT):
        cros_lib.SudoRunCommand(['mkdir', '-p', '%s' % self.CGROUP_ROOT],
            print_cmd=False)

    return True

  def AssertBasicStructure(self):
    """Checks/mounts appropriate sysfs entries."""
    if not self.FileContains(self.MOUNTS_PATH, [self.CGROUP_ROOT]):
      # Not all distros mount cgroup_root to sysfs.
      cros_lib.SudoRunCommand(['mount', '-t', 'tmpfs', 'cgroup_root',
          self.CGROUP_ROOT], print_cmd=False)

    # Mount the root hierarchy.
    opts = ','.join(self.NEEDED_SUBSYSTEMS)
    if not os.path.exists(self.ROOT_PATH):
      # This hierarchy is exclusive to cbuildbot, so it probably doesn't exist.
      cros_lib.SudoRunCommand(['mkdir', '-p', '%s' % self.ROOT_PATH],
          print_cmd=False)

    if not self.FileContains(self.MOUNTS_PATH, [self.ROOT_PATH]):
      # Mount using expected set of opts.
      cros_lib.SudoRunCommand(['mount', '-t', 'cgroup', '-o', opts,
          'cbuildbot', self.ROOT_PATH], print_cmd=False)
# TODO(zbehan): We should not accept pre-existing mount structure without
# checking/fixing it. Not caring for example implies not being able to add
# subsystems. Modifying this requires a remount of the root hierarchy, but
# remounting cgroup filesystems is not yet supported in linux and fails with
# silly messages. Always unmounting and remounting would be an option if we
# assume that there will never be parallel cbuildbot jobs.
#    else:
#      # The root hierarchy already exists, but could be using a different set
#      # of subsystems, remount to make sure.
#      cros_lib.SudoRunCommand(['mount', '-o', 'remount,%s' % opts,
#          self.ROOT_PATH], print_cmd=False)
    # Create a group for jobs. The root of hierarchy is generally read-only
    # and always contains all processes. With a sub-group, we can for
    # instance set default parameters for all newly started cbuildbot jobs.
    if not os.path.exists(self.JOBS_PATH):
      self.InitNamedCGroup(self.JOBS_PATH)

    # Create a dump group. Non-empty hierarchies cannot be removed, so this
    # is the group where the master process has to move itself first before
    # attempting the delete.
    if not os.path.exists(self.DUMP_PATH):
      self.InitNamedCGroup(self.DUMP_PATH)

  def SudoInherit(self, path, name):
    """Set given key to whatever the parent cgroup has."""
    cros_lib.SudoRunCommand(['sh', '-c', 'cat %s > %s' %
        (os.path.join(path, '..', name),
        os.path.join(path, name))], print_cmd=False)

  def SudoSet(self, path, name, value):
    """Set given key of a given cgroup to value."""
    cros_lib.SudoRunCommand(['sh', '-c', '/bin/echo %s > %s' %
        (value, os.path.join(path, name))], print_cmd=False)

  def InitNamedCGroup(self, path):
    """Creates a cgroup given by path."""
    cros_lib.SudoRunCommand(['mkdir', '-p', path], print_cmd=False)
    # Inherit some basics (cpus/mems are needed before we can assign tasks).
    self.SudoInherit(path, 'cpuset.cpus')
    self.SudoInherit(path, 'cpuset.mems')

  def RemoveNamedCGroup(self, path):
    """Removes a cgroup given by path."""
    cros_lib.SudoRunCommand(['rmdir', path], print_cmd=False)

  def AssignPidToGroup(self, path, pid):
    """Assigns a given process to the given cgroup."""
    # Assign this root process to the new cgroup.
    self.SudoSet(path, 'tasks', '%d' % pid)

  def __init__(self, parent=None, disable=False):
    """Constructor.

    args:
      parent - The parent CGroup. Root hierarchy if empty.
    """
    self._disabled = disable
    if disable or not self.cgroup_supported:
      return

    # Calculate derived paths, because CGROUP_ROOT is variable.
    self.ROOT_PATH = os.path.join(self.CGROUP_ROOT, 'cbuildbot')
    self.JOBS_PATH = os.path.join(self.ROOT_PATH, 'jobs')
    self.DUMP_PATH = os.path.join(self.ROOT_PATH, 'dump')

    self.pid = os.getpid()
    if parent:
      parent_path = parent.path
    else:
      parent_path = self.JOBS_PATH
    # Name of the trybot job = pid of the root process.
    self.path = os.path.join(parent_path, '%d' % self.pid)
    self._cgroup_supported = None

  def __enter__(self):
    if self._disabled or not self.cgroup_supported:
      return

    self.AssertBasicStructure()

    # Create a cgroup named as the pid of this process.
    # That would be necessary for multiple running instances of cbuildbot.
    self.InitNamedCGroup(self.path)
    self.AssignPidToGroup(self.path, self.pid)

  def __exit__(self, _type, value, traceback):
    if self._disabled or not self.cgroup_supported:
      return

    # Do not commit suicide - move self to the dump group first.
    self.AssignPidToGroup(self.DUMP_PATH, self.pid)

    # Waiting loop.
    taskfile = os.path.join(self.path, 'tasks')
    steps_passed = 0
    STEP = 1
    # Wait a little for kids to exit normally, then TERM, then KILL. Second
    # KILL is in case anything managed to spawn off some children while first
    # KILL arrived. Give up after 10 seconds after a last kill call.
    INTERVALS = [(5, 'TERM'), (30, 'KILL'), (60, 'KILL'), (100, 'KILL')]
    while True:
      with open(taskfile) as f:
        pids = f.read().splitlines()
        if not pids:
          break

        if not INTERVALS:
          # After all signals and waiting, there are still processes
          # running. Seems like a serious hang.
          print 'CGroup: Failed to clean up children!'
          return

        if INTERVALS and steps_passed == INTERVALS[0][0]:
          cros_lib.SudoRunCommand(['kill', '-%s' % INTERVALS[0][1]] + pids,
              print_cmd=False, error_code_ok=True, redirect_stdout=True,
              combine_stdout_stderr=True)
          INTERVALS.pop(0)

      # Time slept, not passed, it doesn't account for time spent in the
      # above code, but precision is not a priority here.
      steps_passed += STEP
      # Each step is 0.1s
      time.sleep(STEP / 10)

    self.RemoveNamedCGroup(self.path)
