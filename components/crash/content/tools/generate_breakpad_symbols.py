#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to generate symbols for a binary suitable for breakpad.

Currently, the tool only supports Linux, Android, and Mac. Support for other
platforms is planned.
"""

import collections
import errno
import glob
import optparse
import os
import Queue
import re
import shutil
import subprocess
import sys
import threading


CONCURRENT_TASKS=4

# The BINARY_INFO tuple describes a binary as dump_syms identifies it.
BINARY_INFO = collections.namedtuple('BINARY_INFO',
                                     ['platform', 'arch', 'hash', 'name'])


def GetCommandOutput(command, env=None):
  """Runs the command list, returning its output (stdout).

  Args:
    command: (list of strings) a command with arguments

    env: (dict or None) optional environment for the command. If None,
      inherits the existing environment, otherwise completely overrides it.

  From chromium_utils.
  """
  devnull = open(os.devnull, 'w')
  proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=devnull,
                          bufsize=1, env=env)
  output = proc.communicate()[0]
  return output


def GetDumpSymsBinary(build_dir=None):
  """Returns the path to the dump_syms binary."""
  DUMP_SYMS = 'dump_syms'
  dump_syms_bin = os.path.join(os.path.expanduser(build_dir), DUMP_SYMS)
  if not os.access(dump_syms_bin, os.X_OK):
    print 'Cannot find %s.' % dump_syms_bin
    return None

  return dump_syms_bin


def Resolve(path, exe_path, loader_path, rpaths):
  """Resolve a dyld path.

  @executable_path is replaced with |exe_path|
  @loader_path is replaced with |loader_path|
  @rpath is replaced with the first path in |rpaths| where the referenced file
      is found
  """
  path = path.replace('@loader_path', loader_path)
  path = path.replace('@executable_path', exe_path)
  if path.find('@rpath') != -1:
    for rpath in rpaths:
      new_path = Resolve(path.replace('@rpath', rpath), exe_path, loader_path,
                         [])
      if os.access(new_path, os.X_OK):
        return new_path
    return ''
  return path


def GetSharedLibraryDependenciesLinux(binary):
  """Return absolute paths to all shared library dependecies of the binary.

  This implementation assumes that we're running on a Linux system."""
  ldd = GetCommandOutput(['ldd', binary])
  lib_re = re.compile('\t.* => (.+) \(.*\)$')
  result = []
  for line in ldd.splitlines():
    m = lib_re.match(line)
    if m:
      result.append(m.group(1))
  return result


def GetDeveloperDirMac():
  """Finds a good DEVELOPER_DIR value to run Mac dev tools.

  It checks the existing DEVELOPER_DIR and `xcode-select -p` and uses
  one of those if the folder exists, and falls back to one of the
  existing system folders with dev tools.

  Returns:
    (string) path to assign to DEVELOPER_DIR env var.
  """
  candidate_paths = []
  if 'DEVELOPER_DIR' in os.environ:
    candidate_paths.append(os.environ['DEVELOPER_DIR'])
  candidate_paths.extend([
    GetCommandOutput(['xcode-select', '-p']).strip(),
    # Most Mac 10.1[0-2] bots have at least one Xcode installed.
    '/Applications/Xcode.app',
    '/Applications/Xcode9.0.app',
    '/Applications/Xcode8.0.app',
    # Mac 10.13 bots don't have any Xcode installed, but have CLI tools as a
    # temporary workaround.
    '/Library/Developer/CommandLineTools',
  ])
  for path in candidate_paths:
    if os.path.exists(path):
      return path
  print 'WARNING: no value found for DEVELOPER_DIR. Some commands may fail.'


def GetSharedLibraryDependenciesMac(binary, exe_path):
  """Return absolute paths to all shared library dependecies of the binary.

  This implementation assumes that we're running on a Mac system."""
  loader_path = os.path.dirname(binary)
  env = os.environ.copy()
  developer_dir = GetDeveloperDirMac()
  if developer_dir:
    env['DEVELOPER_DIR'] = developer_dir
  otool = GetCommandOutput(['otool', '-l', binary], env=env).splitlines()
  rpaths = []
  for idx, line in enumerate(otool):
    if line.find('cmd LC_RPATH') != -1:
      m = re.match(' *path (.*) \(offset .*\)$', otool[idx+2])
      rpaths.append(m.group(1))

  otool = GetCommandOutput(['otool', '-L', binary], env=env).splitlines()
  lib_re = re.compile('\t(.*) \(compatibility .*\)$')
  deps = []
  for line in otool:
    m = lib_re.match(line)
    if m:
      dep = Resolve(m.group(1), exe_path, loader_path, rpaths)
      if dep:
        deps.append(os.path.normpath(dep))
  return deps


def GetSharedLibraryDependencies(options, binary, exe_path):
  """Return absolute paths to all shared library dependecies of the binary."""
  deps = []
  if sys.platform.startswith('linux'):
    deps = GetSharedLibraryDependenciesLinux(binary)
  elif sys.platform == 'darwin':
    deps = GetSharedLibraryDependenciesMac(binary, exe_path)
  else:
    print "Platform not supported."
    sys.exit(1)

  result = []
  build_dir = os.path.abspath(options.build_dir)
  for dep in deps:
    if (os.access(dep, os.X_OK) and
        os.path.abspath(os.path.dirname(dep)).startswith(build_dir)):
      result.append(dep)
  return result


def mkdir_p(path):
  """Simulates mkdir -p."""
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno == errno.EEXIST and os.path.isdir(path):
      pass
    else: raise


def GetBinaryInfoFromHeaderInfo(header_info):
  """Given a standard symbol header information line, returns BINARY_INFO."""
  # header info is of the form "MODULE $PLATFORM $ARCH $HASH $BINARY"
  info_split = header_info.strip().split(' ', 4)
  if len(info_split) != 5 or info_split[0] != 'MODULE':
    return None
  return BINARY_INFO(*info_split[1:])


def GenerateSymbols(options, binaries):
  """Dumps the symbols of binary and places them in the given directory."""

  queue = Queue.Queue()
  print_lock = threading.Lock()

  def _Worker():
    dump_syms = GetDumpSymsBinary(options.build_dir)
    while True:
      should_dump_syms = True
      reason = "no reason"
      binary = queue.get()

      run_once = True
      while run_once:
        run_once = False

        if not dump_syms:
          should_dump_syms = False
          reason = "Could not locate dump_syms executable."
          break

        binary_info = GetBinaryInfoFromHeaderInfo(
            GetCommandOutput([dump_syms, '-i', binary]).splitlines()[0])
        if not binary_info:
          should_dump_syms = False
          reason = "Could not obtain binary information."
          break

        # See if the output file already exists.
        output_path = os.path.join(options.symbols_dir, binary_info.name,
                                   binary_info.hash, binary_info.name + '.sym')
        if os.path.isfile(output_path):
          should_dump_syms = False
          reason = "Symbol file already found."
          break

        # See if there is a symbol file already found next to the binary
        potential_symbol_files = glob.glob('%s.breakpad*' % binary)
        for potential_symbol_file in potential_symbol_files:
          with open(potential_symbol_file, 'rt') as f:
            symbol_info = GetBinaryInfoFromHeaderInfo(f.readline())
          if symbol_info == binary_info:
            mkdir_p(os.path.dirname(output_path))
            shutil.copyfile(potential_symbol_file, output_path)
            should_dump_syms = False
            reason = "Found local symbol file."
            break

      if not should_dump_syms:
        if options.verbose:
          with print_lock:
            print "Skipping %s (%s)" % (binary, reason)
        queue.task_done()
        continue

      if options.verbose:
        with print_lock:
          print "Generating symbols for %s" % binary

      mkdir_p(os.path.dirname(output_path))
      try:
        with open(output_path, 'wb') as f:
          subprocess.check_call([dump_syms, '-r', binary], stdout=f)
      except Exception, e:
        # Not much we can do about this.
        with print_lock:
          print e

      queue.task_done()

  for binary in binaries:
    queue.put(binary)

  for _ in range(options.jobs):
    t = threading.Thread(target=_Worker)
    t.daemon = True
    t.start()

  queue.join()


def main():
  parser = optparse.OptionParser()
  parser.add_option('', '--build-dir', default='',
                    help='The build output directory.')
  parser.add_option('', '--symbols-dir', default='',
                    help='The directory where to write the symbols file.')
  parser.add_option('', '--binary', default='',
                    help='The path of the binary to generate symbols for.')
  parser.add_option('', '--clear', default=False, action='store_true',
                    help='Clear the symbols directory before writing new '
                         'symbols.')
  parser.add_option('-j', '--jobs', default=CONCURRENT_TASKS, action='store',
                    type='int', help='Number of parallel tasks to run.')
  parser.add_option('-v', '--verbose', action='store_true',
                    help='Print verbose status output.')

  (options, _) = parser.parse_args()

  if not options.symbols_dir:
    print "Required option --symbols-dir missing."
    return 1

  if not options.build_dir:
    print "Required option --build-dir missing."
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

  if not GetDumpSymsBinary(options.build_dir):
    return 1

  # Build the transitive closure of all dependencies.
  binaries = set([options.binary])
  queue = [options.binary]
  exe_path = os.path.dirname(options.binary)
  while queue:
    deps = GetSharedLibraryDependencies(options, queue.pop(0), exe_path)
    new_deps = set(deps) - binaries
    binaries |= new_deps
    queue.extend(list(new_deps))

  GenerateSymbols(options, binaries)

  return 0


if '__main__' == __name__:
  sys.exit(main())
