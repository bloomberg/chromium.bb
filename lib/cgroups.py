# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class for managing the Linux cgroup subsystem."""

import errno
import os
import time
import contextlib

from chromite.lib import cros_build_lib as cros_lib
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
# - for example, cros/cbuildbot/42/cros_sdk-34
# - this pattern continues arbitrarily deep, and is autocleaned.
# cros/cbuildbot/%(pid1)i/%(basename_of_pid2)s_%(pid2)i/
#
# An example for cros_sdk (pid 552) would be:
# cros/cros_sdk/552/
# and it's children would be accessible in 552/tasks, or
# would create their own namespace w/in and assign themselves to it.


def _FileContains(filename, strings):
  """Greps a group of expressions, returns whether all were found."""
  with open(filename, 'r') as f:
    contents = f.read()
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
      cros_lib.SafeMakedirs(cls.MOUNT_ROOT, sudo=True)
      cros_lib.SudoRunCommand(['mount', '-t', 'tmpfs', 'cgroup_root',
                              cls.MOUNT_ROOT], print_cmd=False)

    # Mount the root hierarchy.
    if not _FileContains('/proc/mounts', [cls.CGROUP_ROOT]):
      cros_lib.SafeMakedirs(cls.CGROUP_ROOT, sudo=True)
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
    return [x.strip() for x in self.GetValue('tasks', '').splitlines()]

  def GetValue(self, key, default=None):
    """Query a cgroup configuration key from disk.

    If the file doesn't exist, return the given default."""
    try:
      with open(os.path.join(self.path, key)) as f:
        return f.read()
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
      return

    if self.parent is not None:
      self.parent.Instantiate()
    cros_lib.SafeMakedirs(self.path, sudo=True)

    force_inheritance = (self.parent.GetValue('cgroup.clone_children') != '1')

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

  # Since some of this code needs to check/reset this function to be ran,
  # we use a more developer friendly variable name.
  Instantiate._cache_key = '_inited'

  def _SudoSet(self, key, value):
    """Set a cgroup file in this namespace to a specific value"""
    return sudo.SetFileContents(self._LimitName(key, True), value)

  def RemoveThisGroup(self, strict=False):
    """Remove this specific cgroup

    If strict is True, then we must be removed.
    """
    try:
      return self._RemoveGroupOnDisk(self.path, strict=strict)
    finally:
      self._inited = None

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
    cros_lib.SudoRunCommand(['find', path, '-depth', '-type', 'd',
                            '-exec', 'rmdir', '{}', ';'],
                            redirect_stderr=True, error_ok=not strict,
                            print_cmd=False, strict=sudo_strict)

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
  def ContainChildren(self):
    """Context manager for containing children processes.

    This manager creates a job pool derived from this instance, transfers
    the current process into it upon __enter__.

    Any children processes created at that point will inherit our cgroup;
    they can only escape the group if they're running as root and move
    themselves out of this hierarchy.

    Upon __exit__, transfer the current process back to this group, then
    sigterm (progressing to sigkill) any immediate children in the pool,
    finally removing the pool if possible."""
    node = self.AddGroup(str(os.getpid()), autoclean=True)
    try:
      with self.TemporarilySwitchToGroup(node):
        yield
    finally:
      node.KillProcesses()
      node.RemoveThisGroup()

  def KillProcesses(self):
    """Kill all processes in this namespace."""
    # Waiting loop.
    steps_passed = 0
    STEP = 1
    # Wait a little for kids to exit normally, then TERM, then KILL. Second
    # KILL is in case anything managed to spawn off some children while first
    # KILL arrived. Give up after 10 seconds after a last kill call.
    INTERVALS = [(5, 'TERM'), (30, 'KILL'), (60, 'KILL'), (100, 'KILL')]
    while True:
      pids = self.tasks
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
      with open('/proc/%s/cpuset' % (pid,), 'r') as f:
        cpuset = f.read().rstrip('\n')
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


def ContainChildren(process_name, nesting=True):
  """Convenience context manager to create a cgroup for children containment

  See Cgroup.CreateProcessGroup and Cgroup.ContainChildren for specifics.
  If Cgroups aren't supported on this system, this is a noop context manager.
  """
  node = Cgroup.CreateProcessGroup(process_name, nesting=nesting)
  if node is not None:
    return node.ContainChildren()
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
