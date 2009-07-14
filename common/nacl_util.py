#!/usr/bin/python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys

def GetBase():
  base = os.getcwd().split(os.path.sep + "native_client" + os.path.sep).pop(0)
  return os.path.join(base, "native_client")

def GetSelLdr(target):
  sys_platform = sys.platform.lower()
  if sys_platform in ("linux", "linux2"):
    platform = "linux"
    sel_ldr = "sel_ldr"
  elif sys_platform in ("win", "win32", "windows", "cygwin"):
    platform = "win"
    sel_ldr = "sel_ldr.exe"
  elif sys_platform in ("darwin", "mac"):
    platform = "mac"
    sel_ldr = "sel_ldr"
  else:
    print "ERROR: Unsupported platform for nacl demo!"
    return None

  # TODO(nfullagar): add additional platform target support here
  sel_ldr = os.path.join(GetBase(), "scons-out",
      target + "-" + platform + "-x86-32", "staging", sel_ldr)
  if not os.path.exists(sel_ldr):
    return None
  return sel_ldr


def FindSelLdr(target):
  if target != None:
    # target explicitly specified
    sel_ldr = GetSelLdr(target)
  else:
    # target not specified, so look for dbg first, then opt
    sel_ldr = GetSelLdr("dbg")
    if sel_ldr == None:
      sel_ldr = GetSelLdr("opt")
  if sel_ldr == None:
    print "ERROR: Cannot locate sel_ldr executable"
    print "       See documentation for SCons build instructions"
  return sel_ldr


def GetExe(target):
  # TODO(nfullagar): add additional platform target support here
  scons_target = os.path.join(GetBase(), "scons-out", "nacl" + "-x86-32",
      "staging", target)
  if os.path.exists(scons_target):
    return scons_target
  print ("ERROR: Cannot find Native Client executable at %s" % scons_target)
  print ("       See documentation for SCons build instructions")
  return None


def SubProcessCall(*command):
  arglist = command[0]
  ret_val = os.spawnv(os.P_WAIT, arglist[0], arglist)
  return ret_val


def GetArgs(default_args):
  if len(sys.argv) > 1:
    return sys.argv[1:]
  return default_args


def LaunchSelLdr(demo_exe, args, sel_ldr_target = None):
  sel_ldr = FindSelLdr(sel_ldr_target)
  if not sel_ldr or not demo_exe:
    return -1
  print "Launching sel_ldr at %s" % sel_ldr
  print "Using executable at %s" % demo_exe
  print "Args: %s" % repr(args)
  command = [sel_ldr, "-d", "-f", demo_exe]
  if args != None:
    if len(args) > 0:
      command = command + ["--"] + args
  ret_val = SubProcessCall(command)
  return ret_val


if __name__ == '__main__':
  assert len(sys.argv) > 1
  nexe_name = sys.argv[1]
  del sys.argv[1]
  sys.exit(LaunchSelLdr(GetExe(nexe_name), GetArgs(nexe_args)))
