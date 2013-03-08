# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Support for Linux namespaces"""

import ctypes
import ctypes.util
import os


CLONE_FS = 0x00000200
CLONE_FILES = 0x00000400
CLONE_NEWNS = 0x00020000
CLONE_NEWUTS = 0x04000000
CLONE_NEWIPC = 0x08000000
CLONE_NEWUSER = 0x10000000
CLONE_NEWPID = 0x20000000
CLONE_NEWNET = 0x40000000


def Unshare(flags):
  """Binding to the Linux unshare system call. See unshare(2) for details.

  Arguments:
    flags: Namespaces to unshare; bitwise OR of CLONE_* flags.
  Raises:
    OSError: if unshare failed.
  """
  libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)
  if libc.unshare(ctypes.c_int(flags)) != 0:
    e = ctypes.get_errno()
    raise OSError(e, os.strerror(e))
