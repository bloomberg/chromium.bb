# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class for managing the Linux cgroup subsystem."""

import contextlib
import errno
import os
import signal
import time

from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import osutils
from chromite.lib import signals
from chromite.lib import sudo


# Rough hierarchy sketch:
# - all cgroup aware cros code should nest here.
# - No cros code should modify this namespace- this is user/system configurable
# - only.  A release_agent can be specified, although we won't use it.
# cros/
#
# - cbuildbot instances land here only when they're cleaning their task pool.
# - this root namespace is *not* auto-removed; it's left so that user/system
# - configuration is persistant.
# cros/%(process-name)s/
# cros/cbuildbot/
#
# - a cbuildbot job pool, owned by pid.  These are autocleaned.
# cros/cbuildbot/%(pid)i/
#
# - a job pool using process that was invoked by cbuildbot.
# - for example, cros/cbuildbot/42/cros_sdk:34
# - this pattern continues arbitrarily deep, and is autocleaned.
# cros/cbuildbot/%(pid1)i/%(basename_of_pid2)s:%(pid2)i/
#
# An example for cros_sdk (pid 552) would be:
# cros/cros_sdk/552/
# and it's children would be accessible in 552/tasks, or
# would create their own namespace w/in and assign themselves to it.


class _GroupWasRemoved(Exception):
  """Exception representing when a group was unexpectedly removed.

  Via design, this should only be possible when instantiating a new
  pool, but the parent pool has been removed- this means effectively that
  we're supposed to shutdown (either we've been sigterm'd and ignored it,
  or it's imminent)."""


def _FileContains(filename, strings):
  """Greps a group of expressions, returns whether all were found."""
  contents = osutils.ReadFile(filename)
  return all(s in contents for s in strings)


def MemoizedSingleCall(functor):
  """Decorator for simple functor targets, caching the results

  The functor must accept no arguments beyond either a class or self (depending
  on if this is used in a classmethod/instancemethod context).  Results of the
  wrapped method will be written to the class/instance namespace in a specially
  named cached value.  All future invocations will just reuse that value."""
  # TODO(ferringb): rebase to snakeoil.klass.cached* functionality if/when
  # snakeoil occurs.
  def f(obj):
    # Silence idiotic complaint from pylint.
    # pylint: disable=W0212
    key = f._cache_key
    val = getattr(obj, key, None)
    if val is None:
      val = functor(obj)
      setattr(obj, key, val)
    return val

  # Dummy up our wrapper to make it look like what we're wrapping,
  # and expose the underlying docstrings.
  f.__name__ = functor.__name__
  f.__module__ = functor.__module__
  f.__doc__ = functor.__doc__
  f._cache_key = '_%s_cached' % (functor.__name__.lstrip('_'),)
  return f


def EnsureInitialized(functor):
  """Decorator for Cgroup methods to ensure the method is ran only if inited"""

  def f(self, *args, **kwds):
    # pylint: disable=W0212
    self.Instantiate()
    return functor(self, *args, **kwds)

  # Dummy up our wrapper to make it look like what we're wrapping,
  # and expose the underlying docstrings.
  f.__name__ = functor.__name__
  f.__doc__ = functor.__doc__
  f.__module__ = functor.__module__
  return f


class Cgroup(object):

  """Class representing a group in cgroups hierarchy.

  Note the instance may not exist on disk; it will be created as necessary.
  Additionally, because cgroups is kernel maintained (and mutated on the fly
  by processes using it), chunks of this class are /explicitly/ designed to
  always go back to disk and recalculate values.

  Attributes:
    path: Absolute on disk pathway to the cgroup directory.
    tasks: Pids contained in this immediate cgroup, and the owning pids of
      any first level groups nested w/in us.
    all_tasks: All Pids, and owners of nested groups w/in this point in
      the hierarchy.
    nested_groups: The immediate cgroups nested w/in this one.  If this
      cgroup is 'cbuildbot/buildbot', 'cbuildbot' would have a nested_groups
      of [Cgroup('cbuildbot/buildbot')] for example.
    all_nested_groups: All cgroups nested w/in this one, regardless of depth.
    pid_owner: Which pid owns this cgroup, if the cgroup is following cros
      conventions for group naming.
  """

  NEEDED_SUBSYSTEMS = ('cpuset',)
  PROC_PATH = '/proc/cgroups'
  _MOUNT_ROOT_POTENTIALS = ('/sys/fs/cgroup',)
  _MOUNT_ROOT_FALLBACK = '/dev/cgroup'
  CGROUP_ROOT = None
  MOUNT_ROOT = None
  # Whether or not the cgroup implementation does auto inheritance via
  # cgroup.clone_children
  _SUPPORTS_AUTOINHERIT = False

  @classmethod
  @MemoizedSingleCall
  def CgroupsUsable(cls):
    """Function to sanity check if everything is setup to use cgroups"""
    if not cls.CgroupsSupported():
      return False

    if not _FileContains('/proc/mounts', [cls.MOUNT_ROOT]):
      # Not all distros mount cgroup_root to sysfs.
      osutils.SafeMakedirs(cls.MOUNT_ROOT, sudo=True)
      cros_lib.SudoRunCommand(['mount', '-t', 'tmpfs', 'cgroup_root',
                              cls.MOUNT_ROOT], print_cmd=False)

    # Mount the root hierarchy.
    if not _FileContains('/proc/mounts', [cls.CGROUP_ROOT]):
      osutils.SafeMakedirs(cls.CGROUP_ROOT, sudo=True)
      opts = ','.join(cls.NEEDED_SUBSYSTEMS)
      # This hierarchy is exclusive to cros, so it probably doesn't exist.
      cros_lib.SudoRunCommand(['mount', '-t', 'cgroup', '-o', opts,
                              'cros', cls.CGROUP_ROOT], print_cmd=False)
    cls._SUPPORTS_AUTOINHERIT = os.path.exists(
        os.path.join(cls.CGROUP_ROOT, 'cgroup.clone_children'))
    return True

  @classmethod
  @MemoizedSingleCall
  def CgroupsSupported(cls):
    """Sanity check as to whether or not cgroups are supported."""
    # Is the cgroup subsystem even enabled?

    if not os.path.exists(cls.PROC_PATH):
      return False

    # Does it support the subsystems we want?
    if not _FileContains(cls.PROC_PATH, cls.NEEDED_SUBSYSTEMS):
      return False

    for potential in cls._MOUNT_ROOT_POTENTIALS:
      if os.path.exists(potential):
        cls.MOUNT_ROOT = potential
        break
    else:
      cls.MOUNT_ROOT = cls._MOUNT_ROOT_FALLBACK

    cls.CGROUP_ROOT = os.path.join(cls.MOUNT_ROOT, 'cros')
    return True

  def __init__(self, namespace, autoclean=True, lazy_init=False, parent=None,
               _is_root=False, _overwrite=True):
    """Initalize a cgroup instance.

    Arguments:
      namespace: What cgroup namespace is this in?  cbuildbot/1823 for example.
      autoclean: Should this cgroup be removed once unused?
      lazy_init: Should we create the cgroup immediately, or when needed?
      parent:    A Cgroup instance; if the namespace is cbuildbot/1823, then the
        parent *must* be the cgroup instance for namespace cbuildbot.
      _is_root:  Internal option, shouldn't be used by consuming code.
      _overwrite: Internal option, shouldn't be used by consuming code.
    """
    self._inited = None
    self._overwrite = bool(_overwrite)
    if _is_root:
      namespace = '.'
      self._inited = True
    else:
      namespace = os.path.normpath(namespace)
      if parent is None:
        raise ValueError("Either _is_root must be set to True, or parent must "
                         "be non null")
      if namespace in ('.', ''):
        raise ValueError("Invalid namespace %r was given" % (namespace,))

    self.namespace = namespace
    self.autoclean = autoclean
    self.parent = parent

    if not lazy_init:
      self.Instantiate()

  def _LimitName(self, name, for_path=False, multilevel=False):
    """Translation function doing sanity checks on derivative namespaces

    If you're extending this class, you should be using this for any namespace
    operations that pass through a nested group."""
    # We use a fake pathway here, and this code must do so.  To calculate the
    # real pathway requires knowing CGROUP_ROOT, which requires sudo
    # potentially.  Since this code may be invoked just by loading the module,
    # no execution/sudo should occur.  However, if for_path is set, we *do*
    # require CGROUP_ROOT- which is fine, since we sort that on the way out.
    fake_path = os.path.normpath(os.path.join('/fake-path', self.namespace))
    path = os.path.normpath(os.path.join(fake_path, name))

    # Ensure that the requested pathway isn't trying to sidestep what we
    # expect, and in the process it does internal validation checks.
    if not path.startswith(fake_path + '/'):
      raise ValueError("Name %s tried descending through this namespace into"
                       " another; this isn't allowed." % (name,))
    elif path == self.namespace:
      raise ValueError("Empty name %s" % (name,))
    elif os.path.dirname(path) != fake_path and not multilevel:
      raise ValueError("Name %s is multilevel, but disallowed." % (name,))

    # Get the validated/normalized name.
    name = path[len(fake_path):].strip('/')
    if for_path:
      return os.path.join(self.path, name)
    return name

  @property
  def path(self):
    return os.path.abspath(os.path.join(self.CGROUP_ROOT, self.namespace))

  @property
  def tasks(self):
    s = set(x.strip() for x in self.GetValue('tasks', '').splitlines())
    s.update(x.pid_owner for x in self.nested_groups)
    s.discard(None)
    return s

  @property
  def all_tasks(self):
    s = self.tasks
    for group in self.all_nested_groups:
      s.update(group.tasks)
    return s

  @property
  def nested_groups(self):
    targets = []
    path = self.path
    try:
      targets = [x for x in os.listdir(path)
                 if os.path.isdir(os.path.join(path, x))]
    except EnvironmentError, e:
      if e.errno != errno.ENOENT:
        raise

    targets = [self.AddGroup(x, lazy_init=True, _overwrite=False)
               for x in targets]

    # Suppress initialization checks- if it exists on disk, we know it
    # is already initialized.
    for x in targets:
      x._inited = True
    return targets

  @property
  def all_nested_groups(self):
    # Do a depth first traversal.
    def walk(groups):
      for group in groups:
        for subgroup in walk(group.nested_groups):
          yield subgroup
        yield group
    return list(walk(self.nested_groups))

  @property
  @MemoizedSingleCall
  def pid_owner(self):
    # Ensure it's in cros namespace- if it is outside of the cros namespace,
    # we shouldn't make assumptions about the naming convention used.
    if not self.GroupIsAParent(_cros_node):
      return None
    # See documentation at the top of the file for the naming scheme.
    # It's basically "%(program_name)s:%(owning_pid)i" if the group
    # is nested.
    return os.path.basename(self.namespace).rsplit(':', 1)[-1]

  def GroupIsAParent(self, node):
    """Is the given node a parent of us?"""
    parent_path = node.path + '/'
    return self.path.startswith(parent_path)

  def GetValue(self, key, default=None):
    """Query a cgroup configuration key from disk.

    If the file doesn't exist, return the given default."""
    try:
      return osutils.ReadFile(os.path.join(self.path, key))
    except EnvironmentError, e:
      if e.errno != errno.ENOENT:
        raise
      return default

  def AddGroup(self, name, **kwds):
    """Add and return a cgroup nested in this one.

    See __init__ for the supported keywords.  If this isn't a direct child
    (for example this instance is cbuildbot, and the name is 1823/x), it'll
    create the intermediate groups as lazy_init=True and autoclean=True.
    """
    name = self._LimitName(name, multilevel=True)

    chunks = name.split('/', 1)
    immediate = os.path.join(self.namespace, chunks[0])
    if len(chunks) == 1:
      return self.__class__(immediate, parent=self, **kwds)
    parent = self.__class__(immediate, parent=self, lazy_init=True)
    return parent.AddGroup(chunks[-1], **kwds)

  @MemoizedSingleCall
  def Instantiate(self):
    """Ensure this group exists on disk in the cgroup hierarchy"""

    if not self.namespace:
      # If it's the root of the hierarchy, leave it alone.
      return True

    if self.parent is not None:
      self.parent.Instantiate()
    osutils.SafeMakedirs(self.path, sudo=True)

    force_inheritance = True
    if self.parent.GetValue('cgroup.clone_children', '').strip() == '1':
      force_inheritance = False

    if force_inheritance:
      if self._SUPPORTS_AUTOINHERIT:
        # If the cgroup version supports it, flip the auto-inheritance setting
        # on so that cgroups nested here don't have to manually transfer
        # settings
        self._SudoSet('cgroup.clone_children', '1')

      try:
        # TODO(ferringb): sort out an appropriate filter/list for using:
        # for name in os.listdir(parent):
        # rather than just transfering these two values.
        for name in ('cpuset.cpus', 'cpuset.mems'):
          if not self._overwrite:
            # Top level nodes like cros/cbuildbot we don't want to overwrite-
            # users/system may've leveled configuration.  If it's empty,
            # overwrite it in those cases.
            val = self.GetValue(name, '').strip()
            if val:
              continue
          self._SudoSet(name, self.parent.GetValue(name, ''))
      except (EnvironmentError, cros_lib.RunCommandError):
        # Do not leave half created cgroups hanging around-
        # it makes compatibility a pain since we have to rewrite
        # the cgroup each time.  If instantiation fails, we know
        # the group is screwed up, or the instantiaton code is-
        # either way, no reason to leave it alive.
        self.RemoveThisGroup()
        raise

    return True

  # Since some of this code needs to check/reset this function to be ran,
  # we use a more developer friendly variable name.
  Instantiate._cache_key = '_inited'

  def _SudoSet(self, key, value):
    """Set a cgroup file in this namespace to a specific value"""
    try:
      return sudo.SetFileContents(self._LimitName(key, True), value)
    except cros_build_lib.RunCommandError, e:
      raise _GroupWasRemoved(self.namespace, e)

  def RemoveThisGroup(self, strict=False):
    """Remove this specific cgroup

    If strict is True, then we must be removed.
    """
    if self._RemoveGroupOnDisk(self.path, strict=strict):
      self._inited = None
      return True
    return False

  def RemoveGroup(self, name, strict=False):
    """Removes a nested cgroup of ours

    Args
      name: the namespace to remove.
      strict: if False, remove it if possible.  If True, its an error if it
              cannot be removed.
    """
    return self._RemoveGroupOnDisk(self._LimitName(name, for_path=True),
                                   strict)

  @classmethod
  def _RemoveGroupOnDisk(cls, path, strict, sudo_strict=True):
    """Perform the actual group removal.

    Args:
      namespace: The cgroup namespace to remove
      strict: Boolean; if true, then it's an error if the group can't be
        removed.  This can occur if there are still processes in it, or in
        a nested group.
    """
    # Depth first recursively remove our children cgroups, then ourselves.
    # Allow this to fail since currently it's possible for the cleanup code
    # to not fully kill the hierarchy.  Note that we must do just rmdirs,
    # rm -rf cannot be used- it tries to remove files which are unlinkable
    # in cgroup (only namespaces can be removed via rmdir).
    # See Documentation/cgroups/ for further details.
    path = os.path.normpath(path) + '/'
    # Do a sanity check to ensure that we're not touching anything we
    # shouldn't.
    if not path.startswith(cls.CGROUP_ROOT):
      raise RuntimeError("cgroups.py: Was asked to wipe path %s, refusing. "
                         "strict was %r, sudo_strict was %r"
                         % (path, strict, sudo_strict))

    result = cros_lib.SudoRunCommand(
        ['find', path, '-depth', '-type', 'd', '-exec', 'rmdir', '{}', '+'],
        redirect_stderr=True, error_ok=not strict,
        print_cmd=False, strict=sudo_strict)
    if result.returncode == 0:
      return True
    elif not os.path.isdir(path):
      # We were invoked against a nonexistant path.
      return True
    return False

  def TransferCurrentPid(self):
    """Move the current process into this cgroup"""
    self.TransferPid(os.getpid())

  @EnsureInitialized
  def TransferPid(self, pid):
    """Assigns a given process to this cgroup."""
    # Assign this root process to the new cgroup.
    self._SudoSet('tasks', '%d' % pid)

  # TODO(ferringb): convert to snakeoil.weakref.WeakRefFinalizer
  def __del__(self):
    if self.autoclean and self._inited and self.CGROUP_ROOT:
      # Suppress any sudo_strict behaviour, since we may be invoked
      # during interpreter shutdown.
      self._RemoveGroupOnDisk(self.path, False, sudo_strict=False)

  def TemporarilySwitchToNewGroup(self, namespace, **kwds):
    """
    Context manager to create a new cgroup, and temporarily switch into it.
    """
    node = self.AddGroup(namespace, **kwds)
    return self.TemporarilySwitchToGroup(node)

  @contextlib.contextmanager
  def TemporarilySwitchToGroup(self, group):
    """Temporarily move this process into the given group, moving back after.

    Used in a context manager fashion (aka, the with statement)."""
    group.TransferCurrentPid()
    try:
      yield
    finally:
      self.TransferCurrentPid()

  @contextlib.contextmanager
  def ContainChildren(self, pool_name=None):
    """Context manager for containing children processes.

    This manager creates a job pool derived from this instance, transfers
    the current process into it upon __enter__.

    Any children processes created at that point will inherit our cgroup;
    they can only escape the group if they're running as root and move
    themselves out of this hierarchy.

    Upon __exit__, transfer the current process back to this group, then
    sigterm (progressing to sigkill) any immediate children in the pool,
    finally removing the pool if possible.

    If pool_name is given, that name is used rather than os.getpid() for
    the job pool created.

    Finally, note that during cleanup this will suppress SIGINT and SIGTERM
    to ensure that it cleanses any children before returning.
    """

    if pool_name is None:
      pool_name = str(os.getpid())

    run_kill = False
    try:
      # Note; we use lazy init here so that we cannot trigger a
      # _GroupWasRemoved; we want that contained.
      node = self.AddGroup(pool_name, autoclean=True, lazy_init=True)
      try:
        node.TransferCurrentPid()
      except _GroupWasRemoved:
        raise SystemExit(
            "Group %s was removed under our feet; pool shutdown is underway"
            % node.namespace)
      run_kill = True
      yield
    finally:
      with signals.DeferSignals():
        self.TransferCurrentPid()
        if run_kill:
          node.KillProcesses(remove=True)
        else:
          # Non strict since the group may have failed to be created.
          node.RemoveThisGroup(strict=False)

  def KillProcesses(self, poll_interval=0.05, remove=False):
    """Kill all processes in this namespace."""

    my_pid = str(os.getpid())

    def _SignalPids(pids, signum):
      cros_lib.SudoRunCommand(
          ['kill', '-%i' % signum] + sorted(pids),
          print_cmd=False, error_code_ok=True, redirect_stdout=True,
          combine_stdout_stderr=True)

    # First sigterm what we can, exiting after 2 runs w/out seeing pids.
    # Let this phase run for a max of 10 seconds; afterwards, switch to
    # sigkilling.
    time_end = time.time() + 10
    saw_pids, pids = True, set()
    while time.time() < time_end:
      previous_pids = pids
      pids = self.tasks

      if my_pid in pids:
        raise Exception("Bad API usage: asked to kill cgroup %s, but "
                        "current pid %s is in that group.  Effectively "
                        "asked to kill ourselves."
                        % (self.namespace, my_pid))

      if not pids:
        if not saw_pids:
          break
        saw_pids = False
      else:
        saw_pids = True
        new_pids = pids.difference(previous_pids)
        if new_pids:
          _SignalPids(new_pids, signal.SIGTERM)
          # As long as new pids keep popping up, skip sleeping and just keep
          # stomping them as quickly as possible (whack-a-mole is a good visual
          # analogy of this).  We do this to ensure that fast moving spawns
          # are dealt with as quickly as possible.  When considering this code,
          # it's best to think about forkbomb scenarios- shouldn't occur, but
          # synthetic fork-bombs can occur, thus this code being aggressive.
          continue

      time.sleep(poll_interval)

    # Next do a sigkill scan.  Again, exit only after no pids have been seen
    # for two scans, and all groups are removed.
    groups_existed = True
    while True:
      pids = self.all_tasks

      if pids:
        if my_pid in pids:
          raise Exception("Bad API usage: asked to kill cgroup %s, but "
                          "current pid %i is in that group.  Effectively "
                          "asked to kill ourselves."
                          % (self.namespace, my_pid))

        _SignalPids(pids, signal.SIGKILL)
        saw_pids = True
      elif not (saw_pids or groups_existed):
        break
      else:
        saw_pids = False

      time.sleep(poll_interval)

      # Note this is done after the sleep; try to give the kernel time to
      # shutdown the processes.  They may still be transitioning to defunct
      # kernel side by when we hit this scan, but that's fine- the next will
      # get it.
      # This needs to be nonstrict; it's possible the kernel is currently
      # killing the pids we've just sigkill'd, thus the group isn't removable
      # yet.  Additionally, it's possible a child got forked we didn't see.
      # Ultimately via our killing/removal attempts, it will be removed,
      # just not necessarily on the first run.
      if remove:
        if self.RemoveThisGroup(strict=False):
          # If we successfully removed this group, then there can be no pids,
          # sub groups, etc, within it.  No need to scan further.
          return True
        groups_existed = True
      else:
        groups_existed = [group.RemoveThisGroup(strict=False)
                          for group in self.nested_groups]
        groups_existed = not all(groups_existed)



  @classmethod
  def _FindCurrentCrosGroup(cls, pid=None):
    """Find and return the cros namespace a pid is currently in.

    If no pid is given, os.getpid() is substituted."""
    if pid is None:
      pid = 'self'
    elif not isinstance(pid, (long, int)):
      raise ValueError("pid must be None, or an integer/long.  Got %r" % (pid,))

    cpuset = None
    try:
      # See the kernels Documentation/filesystems/proc.txt if you're unfamiliar
      # w/ procfs, and keep in mind that we have to work across multiple kernel
      # versions.
      cpuset = osutils.ReadFile('/proc/%s/cpuset' % (pid,)).rstrip('\n')
    except EnvironmentError, e:
      if e.errno != errno.ENOENT:
        raise
      with open('/proc/%s/cgroup' % pid) as f:
        for line in f:
          # First digit is the hierachy index, 2nd is subsytem, 3rd is space.
          # 2:cpuset:/
          # 2:cpuset:/cros/cbuildbot/1234

          line = line.rstrip('\n')
          if not line:
            continue
          line = line.split(':', 2)
          if line[1] == 'cpuset':
            cpuset = line[2]
            break

    if not cpuset or not cpuset.startswith("/cros/"):
      return None
    return cpuset[len("/cros/"):].strip("/")

  @classmethod
  def CreateProcessGroup(cls, process_name, nesting=True):
    """Create and return a cgroup for ourselves nesting if allowed.

    Args:
      process_name: See the hierarchy comments at the start of this module.
        This should basically be the process name- cros_sdk for example,
        cbuildbot, etc.
      nesting: If we're invoked by another cros cgroup aware process,
        should we nest ourselves in their hierarchy?  Generally speaking,
        client code should never have a reason to disable nesting."""

    if not cls.CgroupsUsable():
      return None

    target = None
    if nesting:
      target = cls._FindCurrentCrosGroup()
    if target is None:
      target = process_name

    return _cros_node.AddGroup(target, autoclean=(target!=process_name))


def SimpleContainChildren(process_name, nesting=True):
  """Convenience context manager to create a cgroup for children containment

  See Cgroup.CreateProcessGroup and Cgroup.ContainChildren for specifics.
  If Cgroups aren't supported on this system, this is a noop context manager.
  """
  node = Cgroup.CreateProcessGroup(process_name, nesting=nesting)
  if node is not None:
    name = '%s:%i' % (process_name, os.getpid())
    return node.ContainChildren(name)
  return cros_lib.NoOpContextManager()

# Note that it's fairly important that any module level defined cgroups like
# this need to be autoclean=False.  If autoclean=True, they can trigger
# SudoRunCommand during sys.exit; we really don't want that occurring.
# Regardless, an ephemeral cgroup that is autocleaned really shouldn't be
# at module level- should be in a function scope.  These are the sole
# exceptions to that rule.
_root_node = Cgroup(None, _is_root=True, autoclean=False, lazy_init=True)
_cros_node = _root_node.AddGroup('cros', autoclean=False, lazy_init=True,
                                 _overwrite=False)
