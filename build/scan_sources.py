#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys

"""Header Scanner.

This module will scan a set of input sources for include dependencies.  Use
the command-line switch -Ixxxx to add include paths.  All filenames and paths
are expected and returned with POSIX separators.
"""


debug = False


def DebugPrint(txt):
  if debug: print txt


class PathConverter(object):
  """PathConverter does path manipulates using Posix style pathnames.

  Regardless of the native path type, all inputs and outputs to the path
  functions are with POSIX style separators.
  """
  def ToNativePath(self, pathname):
    return os.path.sep.join(pathname.split('/'))

  def ToPosixPath(self, pathname):
    return '/'.join(pathname.split(os.path.sep))

  def isfile(self, pathname):
    ospath = self.ToNativePath(pathname)
    return os.path.isfile(ospath)

  def getcwd(self):
    return self.ToPosixPath(os.getcwd())

  def isabs(self, pathname):
    ospath = self.ToNativePath(pathname)
    return os.path.isabs(ospath)

  def isdir(self, pathname):
    ospath = self.ToNativePath(pathname)
    return os.path.isdir(ospath)

  def open(self, pathname):
    ospath = self.ToNativePath(pathname)
    return open(ospath)

  def abspath(self, pathname):
    ospath = self.ToNativePath(pathname)
    ospath = os.path.abspath(ospath)
    return self.ToPosixPath(ospath)

  def dirname(self, pathname):
    ospath = self.ToNativePath(pathname)
    ospath = os.path.dirname(ospath)
    return self.ToPosixPath(ospath)


filename_to_relative_cache = {}  # (filepath, basepath) -> relpath
findfile_cache = {}  # (tuple(searchdirs), cwd, file) -> filename/None
pathisfile_cache = {}  # abspath -> boolean, works because fs is static
                       # during a run.


class Resolver(object):
  """Resolver finds and generates relative paths for include files.

  The Resolver object provides a mechanism to to find and convert a source or
  include filename into a relative path based on provided search paths.  All
  paths use POSIX style separator.
  """
  def __init__(self, pathobj=PathConverter()):
    self.search_dirs = []
    self.pathobj = pathobj
    self.cwd = self.pathobj.getcwd()
    self.offs = len(self.cwd)

  def AddOneDirectory(self, pathname):
    """Add an include search path."""
    pathname = self.pathobj.abspath(pathname)
    DebugPrint('Adding DIR: %s' % pathname)
    if pathname not in self.search_dirs:
      if self.pathobj.isdir(pathname):
        self.search_dirs.append(pathname)
      else:
        # We can end up here when using the gyp generator analyzer. To avoid
        # spamming only log if debug enabled.
        DebugPrint('Not a directory: %s\n' % pathname)
        return False
    return True

  def RemoveOneDirectory(self, pathname):
    """Remove an include search path."""
    pathname = self.pathobj.abspath(pathname)
    DebugPrint('Removing DIR: %s' % pathname)
    if pathname in self.search_dirs:
      self.search_dirs.remove(pathname)
    return True

  def AddDirectories(self, pathlist):
    """Add list of space separated directories."""
    failed = False
    dirlist = ' '.join(pathlist)
    for dirname in dirlist.split(' '):
      if not self.AddOneDirectory(dirname):
        failed = True
    return not failed

  def GetDirectories(self):
    return self.search_dirs

  def RealToRelative(self, filepath, basepath):
    """Returns a relative path from an absolute basepath and filepath."""
    cache_key = (filepath, basepath)
    cache_result = None
    if cache_key in filename_to_relative_cache:
      cache_result = filename_to_relative_cache[cache_key]
      return cache_result
    def SlowRealToRelative(filepath, basepath):
      path_parts = filepath.split('/')
      base_parts = basepath.split('/')
      while path_parts and base_parts and path_parts[0] == base_parts[0]:
        path_parts = path_parts[1:]
        base_parts = base_parts[1:]
      rel_parts = ['..'] * len(base_parts) + path_parts
      rel_path = '/'.join(rel_parts)
      return rel_path
    rel_path = SlowRealToRelative(filepath, basepath)
    filename_to_relative_cache[cache_key] = rel_path
    return rel_path

  def FilenameToRelative(self, filepath):
    """Returns a relative path from CWD to filepath."""
    filepath = self.pathobj.abspath(filepath)
    basepath = self.cwd
    return self.RealToRelative(filepath, basepath)

  def FindFile(self, filename):
    """Search for <filename> across the search directories, if the path is not
       absolute.  Return the filepath relative to the CWD or None. """
    cache_key = (tuple(self.search_dirs), self.cwd, filename)
    if cache_key in findfile_cache:
      cache_result = findfile_cache[cache_key]
      return cache_result
    result = None
    def isfile(absname):
      res = pathisfile_cache.get(absname)
      if res is None:
        res = self.pathobj.isfile(absname)
        pathisfile_cache[absname] = res
      return res

    if self.pathobj.isabs(filename):
      if isfile(filename):
        result = self.FilenameToRelative(filename)
    else:
      for pathname in self.search_dirs:
        fullname = '%s/%s' % (pathname, filename)
        if isfile(fullname):
          result = self.FilenameToRelative(fullname)
          break
    findfile_cache[cache_key] = result
    return result


def LoadFile(filename):
  # Catch cases where the file does not exist
  try:
    fd = PathConverter().open(filename)
  except IOError:
    DebugPrint('Exception on file: %s' % filename)
    return ''
  # Go ahead and throw if you fail to read
  return fd.read()


scan_cache = {}  # cache (abs_filename -> include_list)


class Scanner(object):
  """Scanner searches for '#include' to find dependencies."""

  def __init__(self, loader=None):
    regex = r'^\s*\#[ \t]*include[ \t]*[<"]([^>"]+)[>"]'
    self.parser = re.compile(regex, re.M)
    self.loader = loader
    if not loader:
      self.loader = LoadFile

  def ScanData(self, data):
    """Generate a list of includes from this text block."""
    return self.parser.findall(data)

  def ScanFile(self, filename):
    """Generate a list of includes from this filename."""
    abs_filename = os.path.abspath(filename)
    if abs_filename in scan_cache:
      return scan_cache[abs_filename]
    includes = self.ScanData(self.loader(filename))
    scan_cache[abs_filename] = includes
    DebugPrint('Source %s contains:\n\t%s' % (filename, '\n\t'.join(includes)))
    return includes


class WorkQueue(object):
  """WorkQueue contains the list of files to be scanned.

  WorkQueue contains provides a queue of files to be processed.  The scanner
  will attempt to push new items into the queue, which will be ignored if the
  item is already in the queue.  If the item is new, it will be added to the
  work list, which is drained by the scanner.
  """
  def __init__(self, resolver, scanner=Scanner()):
    self.added_set = set()
    self.todo_list = list()
    self.scanner = scanner
    self.resolver = resolver

  def PushIfNew(self, filename):
    """Add this dependency to the list of not already there."""
    DebugPrint('Adding %s' % filename)
    resolved_name = self.resolver.FindFile(filename)
    if not resolved_name:
      DebugPrint('Failed to resolve %s' % filename)
      return
    DebugPrint('Resolvd as %s' % resolved_name)
    if resolved_name in self.added_set:
      return
    self.todo_list.append(resolved_name)
    self.added_set.add(resolved_name)

  def PopIfAvail(self):
    """Fetch the next dependency to search."""
    if not self.todo_list:
      return None
    return self.todo_list.pop()

  def Run(self):
    """Search through the available dependencies until the list becomes empty.
      The list must be primed with one or more source files to search."""
    scan_name = self.PopIfAvail()
    while scan_name:
      includes = self.scanner.ScanFile(scan_name)
      # Add the directory of the current scanned file for resolving includes
      # while processing includes for this file.
      scan_dir = PathConverter().dirname(scan_name)
      added_dir = not self.resolver.AddOneDirectory(scan_dir)
      for include_file in includes:
        self.PushIfNew(include_file)
      if added_dir:
        self.resolver.RemoveOneDirectory(scan_dir)
      scan_name = self.PopIfAvail()
    return self.added_set


def DoMain(argv):
  """Entry point used by gyp's pymod_do_main feature."""
  global debug

  resolver = Resolver()
  files = []

  arg_type = ''
  for arg in argv:
    if arg in ['-I', '-S']:
      arg_type = arg
    elif arg == '-D':
      debug = True
    elif arg_type == '-I':
      # Skip generated include directories. These files may not exist and
      # there should be explicit dependency on the target that generates
      # these files.
      if arg.startswith('$!PRODUCT_DIR'):
        continue
      resolver.AddDirectories([arg])
    elif arg_type == '-S':
      files.append(arg)

  workQ = WorkQueue(resolver)
  for filename in files:
    workQ.PushIfNew(filename)

  sources_set = workQ.Run()

  # If any of the original files requested aren't found, add them anyway.
  # This is so that source files that will be generated are still returned in
  # the program output.
  sources_set = sources_set.union(set(files))

  sorted_list = sorted(sources_set)
  return '\n'.join(sorted_list) + '\n'


def Main():
  result = DoMain(sys.argv[1:])
  sys.stdout.write(result)


if __name__ == '__main__':
  Main()
