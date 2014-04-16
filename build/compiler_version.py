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


compiler_version_cache = {}  # Map from (compiler, tool) -> version.


def GetVersion(compiler, tool):
  tool_output = tool_error = None
  cache_key = (compiler, tool)
  cached_version = compiler_version_cache.get(cache_key)
  if cached_version:
    return cached_version
  try:
    # Note that compiler could be something tricky like "distcc g++".
    if tool == "compiler":
      compiler = compiler + " -dumpversion"
      # 4.6
      version_re = re.compile(r"(\d+)\.(\d+)")
    elif tool == "assembler":
      compiler = compiler + " -Xassembler --version -x assembler -c /dev/null"
      # Unmodified: GNU assembler (GNU Binutils) 2.24
      # Ubuntu: GNU assembler (GNU Binutils for Ubuntu) 2.22
      # Fedora: GNU assembler version 2.23.2
      version_re = re.compile(r"^GNU [^ ]+ .* (\d+).(\d+).*?$", re.M)
    elif tool == "linker":
      compiler = compiler + " -Xlinker --version"
      # Using BFD linker
      # Unmodified: GNU ld (GNU Binutils) 2.24
      # Ubuntu: GNU ld (GNU Binutils for Ubuntu) 2.22
      # Fedora: GNU ld version 2.23.2
      # Using Gold linker
      # Unmodified: GNU gold (GNU Binutils 2.24) 1.11
      # Ubuntu: GNU gold (GNU Binutils for Ubuntu 2.22) 1.11
      # Fedora: GNU gold (version 2.23.2) 1.11
      version_re = re.compile(r"^GNU [^ ]+ .* (\d+).(\d+).*?$", re.M)
    else:
      raise Exception("Unknown tool %s" % tool)

    # Force the locale to C otherwise the version string could be localized
    # making regex matching fail.
    env = os.environ.copy()
    env["LC_ALL"] = "C"
    pipe = subprocess.Popen(compiler, shell=True, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    tool_output, tool_error = pipe.communicate()
    if pipe.returncode:
      raise subprocess.CalledProcessError(pipe.returncode, compiler)

    parsed_output = version_re.match(tool_output)
    result = parsed_output.group(1) + parsed_output.group(2)
    compiler_version_cache[cache_key] = result
    return result
  except Exception, e:
    if tool_error:
      sys.stderr.write(tool_error)
    print >> sys.stderr, "compiler_version.py failed to execute:", compiler
    print >> sys.stderr, e
    return ""


def main(args):
  ret_code, result = ExtractVersion(args)
  if ret_code == 0:
    print result
  return ret_code


def DoMain(args):
  """Hook to be called from gyp without starting a separate python
  interpreter."""
  ret_code, result = ExtractVersion(args)
  if ret_code == 0:
    return result
  raise Exception("Failed to extract compiler version for args: %s" % args)


def ExtractVersion(args):
  tool = "compiler"
  if len(args) == 1:
    tool = args[0]
  elif len(args) > 1:
    print "Unknown arguments!"

  # Check if CXX environment variable exists and if it does use that
  # compiler, otherwise check g++.
  compiler = os.getenv("CXX", "g++")
  if compiler:
    compiler_version = GetVersion(compiler, tool)
    if compiler_version != "":
      return (0, compiler_version)

  return (1, None)


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
