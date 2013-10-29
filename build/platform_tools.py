#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import platform
import sys

def IsWindows():
  return sys.platform == 'win32'

def IsMacOS():
  return sys.platform == 'darwin'

def IsLinux():
  return sys.platform.startswith('linux')

# On all our supported platforms, platform.architecture()[0] returns the bitness
# of the python binary, regardless of the OS or hardware. On OSX, a fat binary
# that supports both will return '64bit' even if it is running in 32-bit mode.
# On posix, platform.machine() and platform.uname()[4] return the same as
# the system utility 'uname -m'.
#  On linux, this is x86_64 for 64-bit distros, and i686 for 32-bit Ubuntu.
#  Using the 'linux32' command on 64-bit ubuntu will also make it show i686.
#  Mac OSX 10.8 returns x86_64 for all the machines we care about. On some older
#  versions it returned i386 even on systems with 64-bit userspace support.
# On Windows, platform.machine() and platform.uname()[4] return 'x86' if the
# python binary is 32-bit and 'AMD64' if the python binary is 64-bit. There
# appears to be no sane way to tell if the host OS is 64-bit (but there are
# some insane ways). For our purposes we currently don't care.

# This is only used by the 32-bit linux pnacl toolchain bot, which just
# tests that a build and run of the 32-bit toolchain works, but we don't ship
# those binaries! TODO(dschuff): get rid of this bot or make it do something
# useful.
def Is64BitLinux():
  return IsLinux() and platform.machine() == 'x86_64'
