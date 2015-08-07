# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions specific to build slaves, shared by several buildbot scripts.
"""

import datetime
import glob
import os
import re
import shutil
import sys
import tempfile
import time

from common import chromium_utils

# These codes used to distinguish true errors from script warnings.
ERROR_EXIT_CODE = 1
WARNING_EXIT_CODE = 88


def SlaveBuildName(chrome_dir):
  """Extracts the build name of this slave (e.g., 'chrome-release') from the
  leaf subdir of its build directory.
  """
  return os.path.basename(SlaveBaseDir(chrome_dir))


def SlaveBaseDir(chrome_dir):
  """Finds the full path to the build slave's base directory (e.g.
  'c:/b/chrome/chrome-release').  This is assumed to be the parent of the
  shallowest 'build' directory in the chrome_dir path.

  Raises chromium_utils.PathNotFound if there is no such directory.
  """
  result = ''
  prev_dir = ''
  curr_dir = chrome_dir
  while prev_dir != curr_dir:
    (parent, leaf) = os.path.split(curr_dir)
    if leaf == 'build':
      # Remember this one and keep looking for something shallower.
      result = parent
    if leaf == 'slave':
      # We are too deep, stop now.
      break
    prev_dir = curr_dir
    curr_dir = parent
  if not result:
    raise chromium_utils.PathNotFound('Unable to find slave base dir above %s' %
                                      chrome_dir)
  return result


def GypFlagIsOn(options, flag):
  value = GetGypFlag(options, flag, False)
  # The values we understand as Off are False and a text zero.
  if value is False or value == '0':
    return False
  return True


def GetGypFlag(options, flag, default=None):
  gclient = options.factory_properties.get('gclient_env', {})
  defines = gclient.get('GYP_DEFINES', '')
  gypflags = dict([(a, c if b == '=' else True) for (a, b, c) in
                   [x.partition('=') for x in defines.split(' ')]])
  if flag not in gypflags:
    return default
  return gypflags[flag]


def LogAndRemoveFiles(temp_dir, regex_pattern):
  """Removes files in |temp_dir| that match |regex_pattern|.
  This function prints out the name of each directory or filename before
  it deletes the file from disk."""
  regex = re.compile(regex_pattern)
  if not os.path.isdir(temp_dir):
    return
  for dir_item in os.listdir(temp_dir):
    if regex.search(dir_item):
      full_path = os.path.join(temp_dir, dir_item)
      print 'Removing leaked temp item: %s' % full_path
      try:
        if os.path.islink(full_path) or os.path.isfile(full_path):
          os.remove(full_path)
        elif os.path.isdir(full_path):
          chromium_utils.RemoveDirectory(full_path)
        else:
          print 'Temp item wasn\'t a file or directory?'
      except OSError, e:
        print >> sys.stderr, e
        # Don't fail.


def RemoveOldSnapshots(desktop):
  """Removes ChromiumSnapshot files more than one day old. Such snapshots are
  created when certain tests timeout (e.g., Chrome Frame integration tests)."""
  # Compute the file prefix of a snapshot created one day ago.
  yesterday = datetime.datetime.now() - datetime.timedelta(1)
  old_snapshot = yesterday.strftime('ChromiumSnapshot%Y%m%d%H%M%S')
  # Collect snapshots at least as old as that one created a day ago.
  to_delete = []
  for snapshot in glob.iglob(os.path.join(desktop, 'ChromiumSnapshot*.png')):
    if os.path.basename(snapshot) < old_snapshot:
      to_delete.append(snapshot)
  # Delete the collected snapshots.
  for snapshot in to_delete:
    print 'Removing old snapshot: %s' % snapshot
    try:
      os.remove(snapshot)
    except OSError, e:
      print >> sys.stderr, e


def RemoveChromeDesktopFiles():
  """Removes Chrome files (i.e. shortcuts) from the desktop of the current user.
  This does nothing if called on a non-Windows platform."""
  if chromium_utils.IsWindows():
    desktop_path = os.environ['USERPROFILE']
    desktop_path = os.path.join(desktop_path, 'Desktop')
    LogAndRemoveFiles(desktop_path, r'^(Chromium|chrome) \(.+\)?\.lnk$')
    RemoveOldSnapshots(desktop_path)


def RemoveJumpListFiles():
  """Removes the files storing jump list history.
  This does nothing if called on a non-Windows platform."""
  if chromium_utils.IsWindows():
    custom_destination_path = os.path.join(os.environ['USERPROFILE'],
                                           'AppData',
                                           'Roaming',
                                           'Microsoft',
                                           'Windows',
                                           'Recent',
                                           'CustomDestinations')
    LogAndRemoveFiles(custom_destination_path, '.+')


def RemoveTempDirContents():
  """Obliterate the entire contents of the temporary directory, excluding
  paths in sys.argv.
  """
  temp_dir = os.path.abspath(tempfile.gettempdir())
  print 'Removing contents of %s' % temp_dir

  print '  Inspecting args for files to skip'
  whitelist = set()
  for i in sys.argv:
    try:
      if '=' in i:
        i = i.split('=')[1]
      low = os.path.abspath(i.lower())
      if low.startswith(temp_dir.lower()):
        whitelist.add(low)
    except TypeError:
      # If the argument is too long, windows will freak out and pop a TypeError.
      pass
  if whitelist:
    print '  Whitelisting:'
    for w in whitelist:
      print '    %r' % w

  start_time = time.time()
  for root, dirs, files in os.walk(temp_dir):
    for f in files:
      p = os.path.join(root, f)
      if p.lower() not in whitelist:
        try:
          os.remove(p)
        except OSError:
          pass
      else:
        print '  Keeping file %r (whitelisted)' % p
    for d in dirs[:]:
      p = os.path.join(root, d)
      if p.lower() not in whitelist:
        try:
          # TODO(iannucci): Make this deal with whitelisted items which are
          # inside of |d|

          # chromium_utils.RemoveDirectory gives access denied error when called
          # in this loop.
          shutil.rmtree(p, ignore_errors=True)
          # Remove it so that os.walk() doesn't try to recurse into
          # a non-existing directory.
          dirs.remove(d)
        except OSError:
          pass
      else:
        print '  Keeping dir %r (whitelisted)' % p
  print '   Removing temp contents took %.1f s' % (time.time() - start_time)


def RemoveChromeTemporaryFiles():
  """A large hammer to nuke what could be leaked files from unittests or
  files left from a unittest that crashed, was killed, etc."""
  # NOTE: print out what is cleaned up so the bots don't timeout if
  # there is a lot to cleanup and also se we see the leaks in the
  # build logs.
  # At some point a leading dot got added, support with and without it.
  kLogRegex = r'^\.?(com\.google\.Chrome|org\.chromium)\.'
  if chromium_utils.IsWindows():
    RemoveTempDirContents()
    RemoveChromeDesktopFiles()
    RemoveJumpListFiles()
  elif chromium_utils.IsLinux():
    LogAndRemoveFiles(tempfile.gettempdir(), kLogRegex)
    LogAndRemoveFiles('/dev/shm', kLogRegex)
  elif chromium_utils.IsMac():
    nstempdir_path = '/usr/local/libexec/nstempdir'
    if os.path.exists(nstempdir_path):
      ns_temp_dir = chromium_utils.GetCommandOutput([nstempdir_path]).strip()
      if ns_temp_dir:
        LogAndRemoveFiles(ns_temp_dir, kLogRegex)
    for i in ('Chromium', 'Google Chrome'):
      # Remove dumps.
      crash_path = '%s/Library/Application Support/%s/Crash Reports' % (
          os.environ['HOME'], i)
      LogAndRemoveFiles(crash_path, r'^.+\.dmp$')
  else:
    raise NotImplementedError(
        'Platform "%s" is not currently supported.' % sys.platform)


def WriteLogLines(logname, lines, perf=None):
  logname = logname.rstrip()
  lines = [line.rstrip() for line in lines]
  for line in lines:
    print '@@@STEP_LOG_LINE@%s@%s@@@' % (logname, line)
  if perf:
    perf = perf.rstrip()
    print '@@@STEP_LOG_END_PERF@%s@%s@@@' % (logname, perf)
  else:
    print '@@@STEP_LOG_END@%s@@@' % logname
