#!/usr/bin/python
# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""Utility functions for finding prebuilt Chrome binaries.
"""

import os.path
import urllib

# /continuous is "greener" than /snapshots, but contains fewer files.
BASE_URL = 'http://build.chromium.org/f/chromium/continuous'

# (os, arch) -> (base_directory, archive_name)
# Used from constructing the full URL for a snapshot
SNAPSHOT_MAP = {
           ('windows', 'x86-32'): ('win', 'chrome-win32.zip'),
           ('mac', 'x86-32'): ('mac', 'chrome-mac.zip'),
           ('linux', 'x86-32'): ('linux', 'chrome-linux.zip'),
           ('linux', 'x86-64'): ('linux64', 'chrome-linux.zip'),
           }


# Download the index file.  An example of the format is as follows:
# linux/2010-09-14/59481
# linux/2010-09-11/59193
# linux/2010-03-18/42078
# linux/2010-06-05/49017
# linux/2009-12-27/35295
# NOTE the date cannot be inferred from the revision number. This in turn means
# that the URL cannot be determined without checking the index.
# NOTE The entries are not sorted.
def GetIndexData(verbose, base_url=None):
  if base_url is None:
    base_url = BASE_URL
  path = base_url + '/all_builds.txt'
  if verbose:
    print 'Getting', path
  index = urllib.urlopen(path)
  return index.read()


# The index file is a list of directories.  Parse the directory names to build
# two-level map: platform -> revision -> directory.
def ParseIndex(data, min_rev, max_rev):
  directories = {}
  for directory in data.split():
    platform, date, rev = directory.split('/')
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


def GetIndex(min_rev, max_rev, verbose, base_url=None):
  data = GetIndexData(verbose, base_url=base_url)
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


# Construct the URL for a binary, given platform and revision.
# Unfortunately, this requires downloading the index file to map rev -> url.
# This is because the continuous builder sticks revisions in directories based
# on the day they were built, which is impossible to infer from the revision.
def GetURL(base_url, os, arch, revision):
  key = (os, arch)
  if key not in SNAPSHOT_MAP:
    raise Exception('%s/%s is not supported.  Update SNAPSHOT_MAP if this '
                    'binary exists.' % key)
  base_dir, archive_name = SNAPSHOT_MAP[key]
  revision = int(revision)

  directories = GetIndex(None, None, False, base_url=base_url)
  if revision not in directories[base_dir]:
    raise Exception('A Chromium binary cannot be found for %s at revision %d - '
                    'run find_chrome_revisions.py to find a better revision'
                    % (base_dir, revision))
  directory = directories[base_dir][revision]
  return '/'.join([base_url, directory, archive_name])


def EvalDepsFile(path):
  scope = {'Var': lambda name: scope['vars'][name]}
  execfile(path, {}, scope)
  return scope


# Scrape the DEPS file to find what revision we are at.
def GetChromeRevision(path=None):
  if path is None:
    path = os.path.join(os.path.split(os.path.split(__file__)[0])[0], 'DEPS')
  scope = EvalDepsFile(path)
  return scope['vars']['chrome_rev']
