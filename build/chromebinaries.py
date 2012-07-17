#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for finding prebuilt Chrome binaries.
"""

import os.path
import re
import urllib

BASE_URL = (
    'https://commondatastorage.googleapis.com/chromium-browser-continuous')

directory_pattern = re.compile('<Prefix>([\w\d]+/[\d]+)/?</Prefix>', re.I)
nextmarker_pattern = re.compile(
    '<NextMarker>([\w\d]+/[\d]+)/?</NextMarker>', re.I)

# (os, arch) -> (base_directory, archive_name, pyautopy_name, pyautolib_name)
# Used from constructing the full URL for a snapshot
SNAPSHOT_MAP = {
           ('windows', 'x86-32'): ('Win',
                                   'chrome-win32.zip',
                                   'chrome-win32.test/pyautolib.py',
                                   'chrome-win32.test/_pyautolib.pyd'),
           ('mac', 'x86-32'): ('Mac',
                               'chrome-mac.zip',
                               'chrome-mac.test/pyautolib.py',
                               'chrome-mac.test/_pyautolib.so',
                               'chrome-mac.test/ffmpegsumo.so'),
           ('linux', 'x86-32'): ('Linux',
                                 'chrome-linux.zip',
                                 'chrome-linux.test/pyautolib.py',
                                 'chrome-linux.test/_pyautolib.so'),
           ('linux', 'x86-64'): ('Linux_x64',
                                 'chrome-linux.zip',
                                 'chrome-linux.test/pyautolib.py',
                                 'chrome-linux.test/_pyautolib.so'),
           }


def GetIndexData(verbose, platform, min_rev, base_url=None):
  if base_url is None:
    base_url = BASE_URL
  # Setup marker based on min_rev.
  if min_rev:
    marker = '%s/%s' % (platform, min_rev)
  else:
    marker = ''
  # Gather result of several queries into total.
  total = []
  while True:
    path = '%s/?delimiter=/&prefix=%s/&marker=%s' % (
        base_url, platform, marker)
    if verbose:
      print 'Getting', path
    # Get one page worth.
    index = urllib.urlopen(path)
    page = index.read()
    index.close()
    total.append(page)
    # Find a next marker item, if any, or stop.
    next_markers = nextmarker_pattern.findall(page)
    if next_markers:
      marker = next_markers[0]
    else:
      break
  return ''.join(total)


# The index file is a list of directories.  Parse the directory names to build
# two-level map: platform -> revision -> directory.
def ParseIndex(data, min_rev, max_rev):
  directories = {}
  for page in data.itervalues():
    for directory in directory_pattern.findall(page):
      platform, rev = directory.split('/')
      rev = int(rev)
      pdict = directories.setdefault(platform, {})
      # NOTE the filter does not prevent pdict from being created.
      # This ensures that if a platform exists, it will at least have an empty
      # dictionary.  If an existing platform does not have an empty dictionary
      # then the FindCommon function will effectively ignore that platform and
      # find revisions in common for platforms that have at least one revision,
      # which is incorrect.
      if ((min_rev is None or rev >= min_rev) and
          (max_rev is None or rev <= max_rev)):
        pdict[rev] = directory
  return directories


# Unfortunately, it is necessary to download the index file to map rev -> url.
# This is because the continuous builder sticks revisions in directories based
# on the day they were built, which is impossible to infer from the revision.
def GetIndex(min_rev, max_rev, verbose, base_url=None):
  data = {}
  for _, v in SNAPSHOT_MAP.iteritems():
    platform = v[0]
    data[platform] = GetIndexData(verbose, platform, min_rev, base_url=base_url)
  return ParseIndex(data, min_rev, max_rev)


# Find the revisions where binaries are available for every platform
def FindCommon(directories):
  common = None
  for pdict in directories.itervalues():
    if common is None:
      common = set(pdict.iterkeys())
    else:
      common.intersection_update(pdict.iterkeys())
  if common is None:
    common = set()
  return common


def GetCommonRevisions(min_rev, max_rev, verbose, base_url=None):
  directories = GetIndex(min_rev, max_rev, verbose, base_url=base_url)
  return FindCommon(directories)


def GetBaseDirectory(base_url, os, arch, revision):
  key = (os, arch)
  if key not in SNAPSHOT_MAP:
    raise Exception('%s/%s is not supported.  Update SNAPSHOT_MAP if this '
                    'binary exists.' % key)
  base_dir = SNAPSHOT_MAP[key][0]
  return '/'.join([base_url, base_dir, str(revision)])


# Construct the URL for a binary, given platform and revision.
def GetChromeURL(base_url, os, arch, revision):
  directory = GetBaseDirectory(base_url, os, arch, revision)
  key = (os, arch)
  archive_name = SNAPSHOT_MAP[key][1]
  return '/'.join([directory, archive_name])


def GetPyAutoURLs( base_url, os, arch, revision):
  directory = GetBaseDirectory(base_url, os, arch, revision)
  key = (os, arch)
  pyauto_files = SNAPSHOT_MAP[key][2:]
  pyauto_files = ['/'.join([directory, f]) for f in pyauto_files]
  return pyauto_files


def EvalDepsFile(path):
  scope = {'Var': lambda name: scope['vars'][name],
           'File': lambda name: name}
  execfile(path, {}, scope)
  return scope


# Scrape the DEPS file to find what revision we are at.
def GetChromeRevision(path=None):
  if path is None:
    path = os.path.join(os.path.split(os.path.split(__file__)[0])[0], 'DEPS')
  scope = EvalDepsFile(path)
  return scope['vars']['chromebinaries_rev']
