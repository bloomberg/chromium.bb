#!/usr/bin/python2.5
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Snapshot Build Bisect Tool

This script bisects the Mac snapshot archive using binary search. It starts at
a bad revision (it will try to guess HEAD) and asks for a last known-good
revision. It will then binary search across this revision range by downloading,
unzipping, and opening Chromium for you. After testing the specific revision,
it will ask you whether it is good or bad before continuing the search.

Currently this only works on Mac, but with some effort it could be ported to
other platforms.
"""

# Base URL to download snapshots from.
BUILD_BASE_URL = \
    "http://build.chromium.org/buildbot/snapshots/chromium-rel-mac"

# Location of the latest build revision number
BUILD_LATEST_URL = "%s/LATEST" % BUILD_BASE_URL

# The location of the builds.
BUILD_ARCHIVE_URL = "/%d/"

# Name of the build archive.
BUILD_ZIP_NAME = "chrome-mac.zip"

# Directory name inside the archive.
BUILD_DIR_NAME = "chrome-mac"

# Name of the executable.
BUILD_EXE_NAME = "Chromium.app"

# URL to the ViewVC commit page.
BUILD_VIEWVC_URL = "http://src.chromium.org/viewvc/chrome?view=rev&revision=%d"

###############################################################################

import math
import os
import re
import shutil
import sys
import urllib

def ParseDirectoryIndex(url):
  """Parses the HTML directory listing into a list of revision numbers."""
  handle = urllib.urlopen(url)
  dirindex = handle.read()
  handle.close()
  return re.findall(r'<a href="([0-9]*)/">\1/</a>', dirindex)

def GetRevList(good, bad):
  """Gets the list of revision numbers between |good| and |bad|."""
  # Download the main revlist.
  revlist = ParseDirectoryIndex(BUILD_BASE_URL)
  revlist = map(int, revlist)
  revlist = filter(lambda r: range(good, bad).__contains__(int(r)), revlist)
  revlist.sort()
  return revlist

def TryRevision(rev):
  """Downloads revision |rev|, unzips it, and opens it for the user to test."""
  # Clear anything that's currently there.
  try:
    os.remove(BUILD_ZIP_NAME)
    shutil.rmtree(BUILD_DIR_NAME, True)
  except Exception, e:
    pass

  # Download the file.
  download_url = BUILD_BASE_URL + (BUILD_ARCHIVE_URL % rev) + BUILD_ZIP_NAME
  try:
    urllib.urlretrieve(download_url, BUILD_ZIP_NAME)
  except Exception, e:
    print("Could not retrieve the download. Sorry.")
    print("Tried to get: %s" % download_url)
    sys.exit(-1)

  # Unzip the file.
  os.system("unzip -q %s" % BUILD_ZIP_NAME)

  # Tell Finder to open the app.
  os.system("open %s/%s" % (BUILD_DIR_NAME, BUILD_EXE_NAME))

def AskIsGoodBuild(rev):
  """Annoyingly ask the user whether build |rev| is good or bad."""
  while True:
    check = raw_input("Build %d [g/b]: " % int(rev))[0]
    if (check == "g" or check  == "b"):
      return (check == "g")
    else:
      print("Just answer the question...")

def main():
  print("chrome-bisect: Perform binary search on the snapshot builds")

  # Pick a starting point, try to get HEAD for this.
  bad_rev = 0
  try:
    nh = urllib.urlopen(BUILD_LATEST_URL)
    latest = int(nh.read())
    nh.close()
    bad_rev = raw_input("Bad revision [HEAD:%d]: " % latest)
    if (bad_rev == ""):
      bad_rev = latest
    bad_rev = int(bad_rev)
  except Exception, e:
    print("Could not determine latest revision. This could be bad...")
    bad_rev = int(raw_input("Bad revision: "))

  # Find out when we were good.
  good_rev = 0
  try:
    good_rev = int(raw_input("Last known good [0]: "))
  except Exception, e:
    pass

  # Get a list of revisions to bisect across.
  revlist = GetRevList(good_rev, bad_rev)

  # If we don't have a |good_rev|, set it to be the first revision possible.
  if good_rev == 0:
    good_rev = revlist[0]

  # These are indexes of |revlist|.
  good = 0
  bad = len(revlist) - 1

  # Binary search time!
  while good < bad:
    candidates = revlist[good:bad]
    num_poss = len(candidates)
    if num_poss > 10:
      print("%d candidates. %d tries left." %
          (num_poss, round(math.log(num_poss, 2))))
    else:
      print("Candidates: %s" % revlist[good:bad])

    # Cut the problem in half...
    test = int((bad - good) / 2) + good
    test_rev = revlist[test]

    # Let the user give this revision a spin.
    TryRevision(test_rev)
    if AskIsGoodBuild(test_rev):
      good = test + 1
    else:
      bad = test

  # We're done. Let the user know the results in an official manner.
  print("You are probably looking for build %d." % revlist[bad])
  print("This is the ViewVC URL for the potential bustage:")
  print(BUILD_VIEWVC_URL % revlist[bad])

if __name__ == '__main__':
  main()
