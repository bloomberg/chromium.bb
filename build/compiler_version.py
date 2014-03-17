#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compiler version checking tool for gcc

Print gcc version as XY if you are running gcc X.Y.*.
This is used to tweak build flags for gcc 4.4.
"""

import os
import re
import subprocess
import sys


def GetVersion(compiler, tool):
  tool_output = tool_error = None
  try:
    # Note that compiler could be something tricky like "distcc g++".
    if tool == "compiler":
      compiler = compiler + " -dumpversion"
      # 4.6
      version_re = re.compile(r"(\d+)\.(\d+)")
    elif tool == "assembler":
      compiler = compiler + " -Xassembler --version -x assembler -c /dev/null"
      # GNU assembler (GNU Binutils for Ubuntu) 2.22
      version_re = re.compile(r"GNU [^ ]+ \(.*\) (\d+).(\d+)")
    elif tool == "linker":
      compiler = compiler + " -Xlinker --version"
      # GNU gold (GNU Binutils for Ubuntu 2.22) 1.11
      version_re = re.compile(r"GNU [^ ]+ \(.*\) (\d+).(\d+)")
    else:
      raise Exception("Unknown tool %s" % tool)

    pipe = subprocess.Popen(compiler, shell=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    tool_output, tool_error = pipe.communicate()
    if pipe.returncode:
      raise subprocess.CalledProcessError(pipe.returncode, compiler)

    result = version_re.match(tool_output)
    return result.group(1) + result.group(2)
  except Exception, e:
    if tool_error:
      sys.stderr.write(tool_error)
    print >> sys.stderr, "compiler_version.py failed to execute:", compiler
    print >> sys.stderr, e
    return ""


def main(args):
  tool = "compiler"
  if len(args) == 1:
    tool = args[0]
  elif len(args) > 1:
    print "Unknown arguments!"

  # Check if CXX environment variable exists and
  # if it does use that compiler.
  cxx = os.getenv("CXX", None)
  if cxx:
    cxxversion = GetVersion(cxx, tool)
    if cxxversion != "":
      print cxxversion
      return 0
  else:
    # Otherwise we check the g++ version.
    gccversion = GetVersion("g++", tool)
    if gccversion != "":
      print gccversion
      return 0

  return 1


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
