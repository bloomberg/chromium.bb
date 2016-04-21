# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Downloads items from the Chromium snapshot archive."""

import json
import os
import platform
import re
import urllib
import urllib2

import util

CHROME_49_REVISION = '369932'
CHROME_50_REVISION = '378110'
CHROME_51_REVISION = '386266'

_SITE = 'http://commondatastorage.googleapis.com'
GS_GIT_LOG_URL = (
    'https://chromium.googlesource.com/chromium/src/+/%s?format=json')
GS_SEARCH_PATTERN = r'Cr-Commit-Position: refs/heads/master@{#(\d+)}'


class Site(object):
  CHROMIUM_SNAPSHOT = _SITE + '/chromium-browser-snapshots'
  CHROMIUM_LINUX = _SITE + '/chromium-linux-archive/chromium.linux'


def GetLatestRevision(site=Site.CHROMIUM_SNAPSHOT):
  """Returns the latest revision (as a string) available for this platform.

  Args:
    site: the archive site to check against, default to the snapshot one.
  """
  url = site + '/%s/LAST_CHANGE'
  return urllib.urlopen(url % _GetDownloadPlatform()).read()


def DownloadChrome(revision, dest_dir, site=Site.CHROMIUM_SNAPSHOT):
  """Downloads the packaged Chrome from the archive to the given directory.

  Args:
    revision: the revision of Chrome to download.
    dest_dir: the directory to download Chrome to.
    site: the archive site to download from, default to the snapshot one.

  Returns:
    The path to the unzipped Chrome binary.
  """
  def GetZipName(revision):
    if util.IsWindows():
      return revision + '/chrome-win32.zip'
    elif util.IsMac():
      return revision + '/chrome-mac.zip'
    elif util.IsLinux():
      if platform.architecture()[0] == '64bit':
        return revision + '/chrome-linux.zip'
      else:
        return 'full-build-linux_' + revision + '.zip'

  def GetDirName():
    if util.IsWindows():
      return 'chrome-win32'
    elif util.IsMac():
      return 'chrome-mac'
    elif util.IsLinux():
      if platform.architecture()[0] == '64bit':
        return 'chrome-linux'
      else:
        return 'full-build-linux'

  def GetChromePathFromPackage():
    if util.IsWindows():
      return 'chrome.exe'
    elif util.IsMac():
      return 'Chromium.app/Contents/MacOS/Chromium'
    elif util.IsLinux():
      return 'chrome-wrapper'

  zip_path = os.path.join(dest_dir, 'chrome-%s.zip' % revision)
  if not os.path.exists(zip_path):
    url = site + '/%s/%s' % (_GetDownloadPlatform(), GetZipName(revision))
    print 'Downloading', url, '...'
    urllib.urlretrieve(url, zip_path)
  util.Unzip(zip_path, dest_dir)
  return os.path.join(dest_dir, GetDirName(), GetChromePathFromPackage())


def _GetDownloadPlatform():
  """Returns the name for this platform on the archive site."""
  if util.IsWindows():
    return 'Win'
  elif util.IsMac():
    return 'Mac'
  elif util.IsLinux():
    if platform.architecture()[0] == '64bit':
      return 'Linux_x64'
    else:
      return 'Linux Builder (dbg)(32)'


def GetLatestSnapshotVersion():
  """Returns the latest commit position or git hash of snapshot build."""
  return GetLatestRevision(GetSnapshotDownloadSite())


def GetLatestSnapshotPosition():
  """Returns the latest commit position of snapshot build."""
  latest_revision = GetLatestSnapshotVersion()
  if util.IsLinux() and platform.architecture()[0] == '32bit':
    return GetCommitPositionFromGitHash(latest_revision)
  else:
    return latest_revision


def GetSnapshotDownloadSite():
  """Returns the site to download snapshot build according to the platform."""
  if util.IsLinux() and platform.architecture()[0] == '32bit':
    return Site.CHROMIUM_LINUX
  else:
    return Site.CHROMIUM_SNAPSHOT


def GetCommitPositionFromGitHash(snapshot_hashcode):
  json_url = GS_GIT_LOG_URL % snapshot_hashcode
  try:
    response = urllib2.urlopen(json_url)
  except urllib2.HTTPError as error:
    util.PrintAndFlush('HTTP Error %d' % error.getcode())
    return None
  except urllib2.URLError as error:
    util.PrintAndFlush('URL Error %s' % error.message)
    return None
  data = json.loads(response.read()[4:])
  if 'message' in data:
    message = data['message'].split('\n')
    message = [line for line in message if line.strip()]
    search_pattern = re.compile(GS_SEARCH_PATTERN)
    result = search_pattern.search(message[len(message)-1])
    if result:
      return result.group(1)
  util.PrintAndFlush('Failed to get commit position number for %s' %
                     snapshot_hashcode)
  return None
