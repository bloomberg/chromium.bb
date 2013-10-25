#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to generate symbols for a binary suitable for breakpad.

Currently, the tool only supports Linux and Android. Support for other
platforms is planned.
"""

import errno
import optparse
import os
import re
import shutil
import subprocess
import sys


def GetCommandOutput(command):
  """Runs the command list, returning its output.

  Prints the given command (which should be a list of one or more strings),
  then runs it and returns its output (stdout) as a string.

  From chromium_utils.
  """
  devnull = open(os.devnull, 'w')
  proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=devnull,
                          bufsize=1)
  output = proc.communicate()[0]
  return output


def GetDumpSymsBinary(dump_syms_dir=None):
  """Returns the path to the dump_syms binary."""
  DUMP_SYMS = 'dump_syms'
  dump_syms_bin = None
  if dump_syms_dir:
    bin = os.path.join(os.path.expanduser(dump_syms_dir), DUMP_SYMS)
    if os.access(bin, os.X_OK):
      dump_syms_bin = bin
  else:
    for path in os.environ['PATH'].split(':'):
      bin = os.path.join(path, DUMP_SYMS)
      if os.access(bin, os.X_OK):
        dump_syms_bin = bin
        break
  if not dump_syms_bin:
    print 'Cannot find %s.' % DUMP_SYMS
    sys.exit(1)

  return dump_syms_bin


def FindLib(lib, rpaths):
  """Resolves the given library relative to a list of rpaths."""
  if lib.find('@rpath') == -1:
    return lib
  for rpath in rpaths:
    real_lib = re.sub('@rpath', rpath, lib)
    if os.access(real_lib, os.X_OK):
      return real_lib

  print 'Could not find "%s"' % lib
  return None


def GetSharedLibraryDependencies(binary):
  """Return absolute paths to all shared library dependecies of the binary."""
  ldd = GetCommandOutput(['ldd', binary])
  lib_re = re.compile('^\t.* => (.+) \(.*\)$')
  result = []
  for line in ldd.splitlines():
    m = lib_re.match(line)
    if m:
      result.append(m.group(1))
  return result


def mkdir_p(path):
  """Simulates mkdir -p."""
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno == errno.EEXIST and os.path.isdir(path):
      pass
    else: raise


def GenerateSymbols(dump_syms_dir, symbol_dir, binary):
  """Dumps the symbols of binary and places them in the given directory."""
  syms = GetCommandOutput([GetDumpSymsBinary(dump_syms_dir), binary])
  module_line = re.match("^MODULE [^ ]+ [^ ]+ ([0-9A-F]+) (.*)\n", syms)
  output_path = os.path.join(symbol_dir, module_line.group(2),
                             module_line.group(1))
  mkdir_p(output_path)
  symbol_file = "%s.sym" % module_line.group(2)
  f = open(os.path.join(output_path, symbol_file), 'w')
  f.write(syms)


def main():
  if not sys.platform.startswith('linux'):
    print "Currently only supported on Linux."
    return 1

  parser = optparse.OptionParser()
  parser.add_option('', '--dump-syms-dir', default='',
                    help='The directory where dump_syms is installed. '
                         'Searches $PATH by default')
  parser.add_option('', '--symbols-dir', default='',
                    help='The directory where to write the symbols file.')
  parser.add_option('', '--binary', default='',
                    help='The path of the binary to generate symbols for.')
  parser.add_option('', '--clear', default=False, action='store_true',
                    help='Clear the symbols directory before writing new '
                         'symbols.')

  (options, _) = parser.parse_args()

  if not options.symbols_dir:
    print "Required option --symbols-dir missing."
    return 1

  if not options.binary:
    print "Required option --binary missing."
    return 1

  if not os.access(options.binary, os.X_OK):
    print "Cannot find %s." % options.binary
    return 1

  if options.clear:
    try:
      shutil.rmtree(options.symbols_dir)
    except:
      pass

  # Build the transitive closure of all dependencies.
  binaries = set([options.binary])
  queue = [options.binary]
  while queue:
    deps = GetSharedLibraryDependencies(queue.pop(0))
    new_deps = set(deps) - binaries
    binaries |= new_deps
    queue.extend(list(new_deps))

  for binary in binaries:
    GenerateSymbols(options.dump_syms_dir, options.symbols_dir, binary)

  return 0


if '__main__' == __name__:
  sys.exit(main())
