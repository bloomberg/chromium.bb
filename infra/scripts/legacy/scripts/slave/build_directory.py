# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for discovering and clearing the build directory."""

import os
import sys


def IsFileNewerThanFile(file_a, file_b):
  """Returns True if file_a's mtime is newer than file_b's."""
  def getmtime(f):
    try:
      return os.path.getmtime(f)
    except os.error:
      return 0
  return getmtime(file_a) >= getmtime(file_b)


def AreNinjaFilesNewerThanXcodeFiles(src_dir=None):
  """Returns True if the generated ninja files are newer than the generated
  xcode files.

  Parameters:
    src_dir: The path to the src directory.  If None, it's assumed to be
             at src/ relative to the current working directory.
  """
  src_dir = src_dir or 'src'
  ninja_path = os.path.join(src_dir, 'out', 'Release', 'build.ninja')
  xcode_path = os.path.join(
      src_dir, 'build', 'all.xcodeproj', 'project.pbxproj')
  return IsFileNewerThanFile(ninja_path, xcode_path)


def AreNinjaFilesNewerThanMSVSFiles(src_dir=None):
  """Returns True if the generated ninja files are newer than the generated
  msvs files.

  Parameters:
    src_dir: The path to the src directory.  If None, it's assumed to be
             at src/ relative to the current working directory.
  """
  src_dir = src_dir or 'src'
  ninja_path = os.path.join(src_dir, 'out', 'Release', 'build.ninja')
  msvs_path = os.path.join(src_dir, 'build', 'all.sln')
  return IsFileNewerThanFile(ninja_path, msvs_path)


def GetBuildOutputDirectory(src_dir=None, cros_board=None):
  """Returns the path to the build directory, relative to the checkout root.

  Assumes that the current working directory is the checkout root.
  """
  # src_dir is only needed for compiling v8, which uses compile.py (but no other
  # of the build scripts), but its source root isn't "src" -- crbug.com/315004
  if src_dir is None:
    src_dir = 'src'

  if sys.platform.startswith('linux'):
    out_dirname = 'out'
    if cros_board:
      # Simple chrome workflow output (e.g., "out_x86-generic")
      out_dirname += '_%s' % (cros_board,)
    return os.path.join(src_dir, out_dirname)
  assert not cros_board, "'cros_board' not supported on this platform"

  if sys.platform == 'darwin':
    if AreNinjaFilesNewerThanXcodeFiles(src_dir):
      return os.path.join(src_dir, 'out')
    return os.path.join(src_dir, 'xcodebuild')

  if sys.platform == 'cygwin' or sys.platform.startswith('win'):
    if AreNinjaFilesNewerThanMSVSFiles(src_dir):
      return os.path.join(src_dir, 'out')
    return os.path.join(src_dir, 'build')

  raise NotImplementedError('Unexpected platform %s' % sys.platform)


def RmtreeExceptNinjaOrGomaFiles(build_output_dir):
  """Recursively removes everything but ninja files from a build directory."""
  for root, _, files in os.walk(build_output_dir, topdown=False):
    for f in files:
      # For .manifest in particular, gyp windows ninja generates manifest
      # files at generation time but clobber nukes at the beginning of
      # compile, so make sure not to delete those generated files, otherwise
      # compile will fail.
      if (f.endswith('.ninja') or f.endswith('.manifest') or
          f == 'args.gn' or
          f.startswith('msvc') or  # VS runtime DLLs.
          f.startswith('pgort') or  # VS PGO runtime DLL.
          f in ('gyp-mac-tool', 'gyp-win-tool',
                'environment.x86', 'environment.x64')):
        continue
      # Keep goma related files.
      if f == '.goma_deps':
        continue
      os.unlink(os.path.join(root, f))
    # Delete the directory if empty; this works because the walk is bottom-up.
    try:
      os.rmdir(root)
    except OSError, e:
      if e.errno in (39, 41, 66):
        # If the directory isn't empty, ignore it.
        # On Windows, os.rmdir will raise WindowsError with winerror 145,
        # which e.errno is 41.
        # On Linux, e.errno is 39.
        pass
      else:
        raise
