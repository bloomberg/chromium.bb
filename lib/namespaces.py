# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Support for Linux namespaces"""

from __future__ import print_function

import ctypes
import ctypes.util
import errno
import os

from chromite.lib import osutils
from chromite.lib import process_util


CLONE_FS = 0x00000200
CLONE_FILES = 0x00000400
CLONE_NEWNS = 0x00020000
CLONE_NEWUTS = 0x04000000
CLONE_NEWIPC = 0x08000000
CLONE_NEWUSER = 0x10000000
CLONE_NEWPID = 0x20000000
CLONE_NEWNET = 0x40000000


def SetNS(fd, nstype):
  """Binding to the Linux setns system call. See setns(2) for details.

  Args:
    fd: An open file descriptor or path to one.
    nstype: Namespace to enter; one of CLONE_*.

  Raises:
    OSError: if setns failed.
  """
  try:
    fp = None
    if isinstance(fd, basestring):
      fp = open(fd)
      fd = fp.fileno()

    libc = ctypes.CDLL(ctypes.util.find_library('c'), use_errno=True)
    if libc.setns(ctypes.c_int(fd), ctypes.c_int(nstype)) != 0:
      e = ctypes.get_errno()
      raise OSError(e, os.strerror(e))
  finally:
    if fp is not None:
      fp.close()


def Unshare(flags):
  """Binding to the Linux unshare system call. See unshare(2) for details.

  Args:
    flags: Namespaces to unshare; bitwise OR of CLONE_* flags.

  Raises:
    OSError: if unshare failed.
  """
  libc = ctypes.CDLL(ctypes.util.find_library('c'), use_errno=True)
  if libc.unshare(ctypes.c_int(flags)) != 0:
    e = ctypes.get_errno()
    raise OSError(e, os.strerror(e))


def _ReapChildren(pid):
  """Reap all children that get reparented to us until we see |pid| exit.

  Args:
    pid: The main child to watch for.

  Returns:
    The wait status of the |pid| child.
  """
  pid_status = 0

  while True:
    try:
      (wpid, status) = os.wait()
      if pid == wpid:
        # Save the status of our main child so we can exit with it below.
        pid_status = status
    except OSError as e:
      if e.errno == errno.ECHILD:
        break
      else:
        raise

  return pid_status


def CreatePidNs():
  """Start a new pid namespace

  This will launch all the right manager processes.  The child that returns
  will be isolated in a new pid namespace.

  If functionality is not available, then it will return w/out doing anything.

  Returns:
    The last pid outside of the namespace.
  """
  first_pid = os.getpid()

  try:
    # First create the namespace.
    Unshare(CLONE_NEWPID)
  except OSError as e:
    if e.errno == errno.EINVAL:
      # For older kernels, or the functionality is disabled in the config,
      # return silently.  We don't want to hard require this stuff.
      return first_pid
    else:
      # For all other errors, abort.  They shouldn't happen.
      raise

  # Now that we're in the new pid namespace, fork.  The parent is the master
  # of it in the original namespace, so it only monitors the child inside it.
  # It is only allowed to fork once too.
  pid = os.fork()
  if pid:
    # Reap the children as the parent of the new namespace.
    process_util.ExitAsStatus(_ReapChildren(pid))
  else:
    # The child needs its own proc mount as it'll be different.
    osutils.Mount('proc', '/proc', 'proc',
                  osutils.MS_NOSUID | osutils.MS_NODEV | osutils.MS_NOEXEC |
                  osutils.MS_RELATIME)

    pid = os.fork()
    if pid:
      # Watch all of the children.  We need to act as the master inside the
      # namespace and reap old processes.
      process_util.ExitAsStatus(_ReapChildren(pid))

  # The grandchild will return and take over the rest of the sdk steps.
  return first_pid
