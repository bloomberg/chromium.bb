#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Author: mpcomplete
#
# This script updates and does a clean build of chrome for you.
# Usage: python chrome-update.py C:\path\to\chrome\trunk
#
# It assumes the following:
# - You have gclient.bat and devenv.com in your path (use the wrapper batch
#   file to ensure this).

import sys
import os
import subprocess
import httplib
import re
import shutil
import optparse

def Message(str):
  """Prints a status message."""
  print "[chrome-update]", str

def FixupPath(path):
  """Returns the OS-ified version of a windows path."""
  return os.path.sep.join(path.split("\\"))

def GetRevision():
  """Returns the revision number of the last build that was archived, or
  None on failure."""
  HOST = "build.chromium.org"
  PATH = "/buildbot/continuous/LATEST/REVISION"
  EXPR = r"(\d+)"

  connection = httplib.HTTPConnection(HOST)
  connection.request("GET", PATH)
  response = connection.getresponse()
  text = response.read()
  match = re.search(EXPR, text)
  if match:
    return int(match.group(1))
  return None

def SetRevisionForUpdate(chrome_root):
  """Prepares environment so gclient syncs to a good revision, if possible."""
  # Find a buildable revision.
  rev = GetRevision()
  if rev == None:
    Message("WARNING: Failed to find a buildable revision.  Syncing to trunk.")
    return "trunk"

  # Read the .gclient file.
  gclient_file = chrome_root + FixupPath("\\.gclient")
  if not os.path.exists(gclient_file):
    Message("WARNING: Failed to find .gclient file.  Syncing to trunk.")
    return "trunk"
  scope = {}
  execfile(gclient_file, scope)
  solutions = scope["solutions"]

  # Edit the url of the chrome 'src' solution, unless the user wants a
  # specific revision.
  for solution in solutions:
    if solution["name"] == "src":
      splitter = solution["url"].split("@")
      if len(splitter) == 1:
        solution["url"] = splitter[0] + "@" + str(rev)
      else:
        rev = int(splitter[1])
      break

  # Write out the new .gclient file.
  gclient_override = gclient_file + "-update-chrome"
  f = open(gclient_override, "w")
  f.write("solutions = " + str(solutions))
  f.close()

  # Set the env var that the gclient tool looks for.
  os.environ["GCLIENT_FILE"] = gclient_override
  return rev

def DoUpdate(chrome_root):
  """gclient sync to the latest build."""
  # gclient sync
  rev = SetRevisionForUpdate(chrome_root)

  cmd = ["gclient.bat", "sync"]
  Message("Updating to %s: %s" % (rev, cmd))
  sys.stdout.flush()
  return subprocess.call(cmd, cwd=chrome_root)

def DoClean(chrome_root, type):
  """Clean our build dir."""
  # rm -rf src/chrome/Debug
  rv = [0]
  def onError(func, path, excinfo):
    Message("Couldn't remove '%s': %s" % (path, excinfo))
    rv[0] = [1]

  build_path = chrome_root + FixupPath("\\src\\chrome\\" + type)
  Message("Cleaning: %s" % build_path)
  shutil.rmtree(build_path, False, onError)
  return rv[0]

def DoBuild(chrome_root, chrome_sln, clean, type):
  """devenv /build what we just checked out."""
  if clean:
    rv = DoClean(chrome_root, type)
    if rv != 0:
      Message("WARNING: Clean failed.  Doing a build without clean.")

  # devenv chrome.sln /build Debug
  cmd = ["devenv.com", chrome_sln, "/build", type]

  Message("Building: %s" % cmd)
  sys.stdout.flush()
  return subprocess.call(cmd, cwd=chrome_root)

def Main():
  parser = optparse.OptionParser()
  parser.add_option("", "--clean", action="store_true", default=False,
                    help="wipe Debug output directory before building")
  parser.add_option("", "--solution", default="src\\chrome\\chrome.sln",
                    help="path to the .sln file to build (absolute, or "
                         "relative to chrome trunk")
  parser.add_option("", "--release", action="store_true", default=False,
                    help="build the release configuration in addition of the "
                         "debug configuration.")
  parser.add_option("", "--nosync", action="store_true", default=False,
                    help="doesn't sync before building")
  parser.add_option("", "--print-latest", action="store_true", default=False,
                    help="print the latest buildable revision and exit")
  options, args = parser.parse_args()

  if options.print_latest:
    print GetRevision() or "HEAD"
    sys.exit(0)

  if not args:
    Message("Usage: %s <path\\to\\chrome\\root> [options]" % sys.argv[0])
    sys.exit(1)

  chrome_root = args[0]
  if not os.path.isdir(chrome_root):
    Message("Path to chrome root (%s) not found." % repr(chrome_root))
    sys.exit(1)

  if not options.nosync:
    rv = DoUpdate(chrome_root)
    if rv != 0:
      Message("Update Failed.  Bailing.")
      sys.exit(rv)

  chrome_sln = FixupPath(options.solution)
  rv = DoBuild(chrome_root, chrome_sln, options.clean, "Debug")
  if rv != 0:
    Message("Debug build failed.  Sad face :(")

  if options.release:
    rv = DoBuild(chrome_root, chrome_sln, options.clean, "Release")
    if rv != 0:
      Message("Release build failed.  Sad face :(")

  if rv != 0:
    sys.exit(rv)

  Message("Success!")

if __name__ == "__main__":
  Main()
