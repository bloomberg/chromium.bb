#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that reads omahaproxy and gsutil to determine version of SDK to put
in manifest.
"""

import buildbot_common
import csv
import manifest_util
import os
import re
import subprocess
import sys
import time
import urllib2


# TODO(binji) handle pushing pepper_trunk


BUCKET_PATH = 'nativeclient-mirror/nacl/nacl_sdk'
GS_SDK_MANIFEST = 'gs://%s/naclsdk_manifest2.json' % (BUCKET_PATH,)


def SplitVersion(version_string):
  """Split a version string (e.g. "18.0.1025.163") into its components.
  
  Note that this function doesn't handle versions in the form "trunk.###".
  """
  return tuple(map(int, version_string.split('.')))


def JoinVersion(version_tuple):
  """Create a string from a version tuple.

  The tuple should be of the form (18, 0, 1025, 163).
  """
  return '.'.join(map(str, version_tuple))


def GetChromeRepoSDKManifest():
  with open(os.path.join('json', 'naclsdk_manifest2.json'), 'r') as sdk_stream:
    sdk_json_string = sdk_stream.read()

  manifest = manifest_util.SDKManifest()
  manifest.LoadDataFromString(sdk_json_string)
  return manifest


def GetChromeVersionHistory():
  """Read Chrome version history from omahaproxy.appspot.com/history.

  Here is an example of data from this URL:
    cros,stable,18.0.1025.168,2012-05-01 17:04:05.962578\n
    win,canary,20.0.1123.0,2012-05-01 13:59:31.703020\n
    mac,canary,20.0.1123.0,2012-05-01 11:54:13.041875\n
    win,stable,18.0.1025.168,2012-04-30 20:34:56.078490\n
    mac,stable,18.0.1025.168,2012-04-30 20:34:55.231141\n
    ...
  Where each line has comma separated values in the following format:
  platform, channel, version, date/time\n

  Returns:
    A list where each element is a line from the document, represented as a
    tuple. The version number has also been split at '.', and converted to ints
    for easier comparisons.
  """
  url_stream = urllib2.urlopen('https://omahaproxy.appspot.com/history')
  return [(platform, channel, SplitVersion(version), date)
      for platform, channel, version, date in csv.reader(url_stream)]


def GetPlatformMajorVersionHistory(history, with_platform, with_major_version):
  """Yields Chrome history for a given platform and major version.

  Args:
    history: Chrome update history as returned from GetChromeVersionHistory.
    with_platform: The name of the platform to filter for.
    with_major_version: The major version to filter for.
  Returns:
    A generator that yields a tuple (channel, version) for each version that
    matches the platform and major version. The version returned is a tuple as
    returned from SplitVersion.
  """
  for platform, channel, version, _ in history:
    if with_platform == platform and with_major_version == version[0]:
      yield channel, version


def FindNextSharedVersion(history, major_version, platforms):
  """Yields versions of Chrome that exist on all given platforms, in order of
     newest to oldest.

  Versions are compared in reverse order of release. That is, the most recently
  updated version will be tested first.

  Args:
    history: Chrome update history as returned from GetChromeVersionHistory.
    major_version: The major version to filter for.
    platforms: A sequence of platforms to filter for. Any other platforms will
        be ignored.
  Returns:
    A generator that yields a tuple (version, channel) for each version that
    matches all platforms and the major version. The version returned is a
    string (e.g. "18.0.1025.164").
  """
  platform_generators = []
  for platform in platforms:
    platform_generators.append(GetPlatformMajorVersionHistory(history, platform,
                                                              major_version))

  shared_version = None
  platform_versions = [(tuple(), '')] * len(platforms)
  while True:
    try:
      for i, platform_gen in enumerate(platform_generators):
        if platform_versions[i][1] != shared_version:
            platform_versions[i] = platform_gen.next()
    except StopIteration:
      return

    shared_version = min(v for c, v in platform_versions)

    if all(v == shared_version for c, v in platform_versions):
      # grab the channel from an arbitrary platform
      first_platform = platform_versions[0]
      channel = first_platform[0]
      yield JoinVersion(shared_version), channel

      # force increment to next version for all platforms
      shared_version = None


def GetAvailableNaClSDKArchivesFor(version_string):
  """Downloads a list of all available archives for a given version.

  Args:
    version_string: The version to find archives for. (e.g. "18.0.1025.164")
  Returns:
    A list of strings, each of which is a platform-specific archive URL. (e.g.
    "https://commondatastorage.googleapis.com/nativeclient_mirror/nacl/"
    "nacl_sdk/18.0.1025.164/naclsdk_linux.bz2".
  """
  gsutil = buildbot_common.GetGsutil()
  path = 'gs://%s/%s' % (BUCKET_PATH, version_string,)
  cmd = [gsutil, 'ls', path]
  process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  stdout, stderr = process.communicate()
  if process.returncode != 0:
    raise subprocess.CalledProcessError(process.returncode, ''.join(cmd))

  # filter out empty lines
  files = filter(lambda x: x, stdout.split('\n'))
  archives = [file for file in files if not file.endswith('.json')]
  manifests = [file for file in files if file.endswith('.json')]

  # don't include any archives that don't have an associated manifest.
  return filter(lambda a: a + '.json' in manifests, archives)


def GetPlatformsFromArchives(archives):
  """Get the platform names for a sequence of archives.

  Args:
    archives: A sequence of archives.
  Returns:
    A list of platforms, one for each archive in |archives|."""
  platforms = []
  for path in archives:
    match = re.match(r'naclsdk_(.*)\.bz2', os.path.basename(path))
    if match:
      platforms.append(match.group(1))
  return platforms


def GetPlatformArchiveBundle(archive):
  """Downloads the manifest "snippet" for an archive, and reads it as a Bundle.

  Args:
    archive: The URL of a platform-specific archive.
  Returns:
    An object of type manifest_util.Bundle, read from a JSON file storing
    metadata for this archive.
  """
  gsutil = buildbot_common.GetGsutil()
  cmd = [gsutil, 'cat', archive + '.json']
  process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  stdout, stderr = process.communicate()
  if process.returncode != 0:
    return None

  bundle = manifest_util.Bundle('')
  bundle.LoadDataFromString(stdout)
  return bundle


def GetTimestampManifestName():
  """Create a manifest name with a timestamp.

  Returns:
    A manifest name with an embedded date. This should make it easier to roll
    back if necessary.
  """
  return time.strftime('naclsdk_manifest2.%Y_%m_%d_%H_%M_%S.json',
      time.gmtime())


def UploadManifest(manifest):
  """Upload a serialized manifest_util.SDKManifest object.

  Upload one copy to gs://<BUCKET_PATH>/naclsdk_manifest2.json, and a copy to
  gs://<BUCKET_PATH>/manifest_backups/naclsdk_manifest2.<TIMESTAMP>.json.

  Args:
    manifest: The new manifest to upload.
  """
  manifest_string = manifest.GetDataAsString()
  gsutil = buildbot_common.GetGsutil()
  timestamp_manifest_path = 'gs://%s/manifest_backups/%s' % (
      BUCKET_PATH, GetTimestampManifestName())

  cmd = [gsutil, 'cp', '-a', 'public-read', '-', timestamp_manifest_path]
  process = subprocess.Popen(cmd, stdin=subprocess.PIPE)
  stdout, stderr = process.communicate(manifest_string)

  # copy from timestampped copy over the official manifest.
  cmd = [gsutil, 'cp', '-a', 'public-read', timestamp_manifest_path,
      GS_SDK_MANIFEST]
  subprocess.check_call(cmd)


def main(args):
  manifest = GetChromeRepoSDKManifest()
  auto_update_major_versions = []
  for bundle in manifest.GetBundles():
    archives = bundle.GetArchives()
    if not archives:
      auto_update_major_versions.append(bundle.version)

  if auto_update_major_versions:
    history = GetChromeVersionHistory()
    platforms = ('mac', 'win', 'linux')

    versions_to_update = []
    for major_version in auto_update_major_versions:
      shared_version_generator = FindNextSharedVersion(history, major_version,
                                                       platforms)
      version = None
      while True:
        try:
          version, channel = shared_version_generator.next()
        except StopIteration:
          raise Exception('No shared version for major_version %s, '
              'platforms: %s. Last version checked = %s' % (
              major_version, ', '.join(platforms), version))

        archives = GetAvailableNaClSDKArchivesFor(version)
        archive_platforms = GetPlatformsFromArchives(archives)
        if set(archive_platforms) == set(platforms):
          versions_to_update.append((major_version, version, channel, archives))
          break

    if versions_to_update:
      for major_version, version, channel, archives in versions_to_update:
        bundle_name = 'pepper_%s' % (major_version,)
        bundle = manifest.GetBundle(bundle_name)
        bundle_recommended = bundle.recommended
        for archive in archives:
          platform_bundle = GetPlatformArchiveBundle(archive)
          bundle.MergeWithBundle(platform_bundle)
        bundle.stability = channel
        bundle.recommended = bundle_recommended
        manifest.MergeBundle(bundle)
    UploadManifest(manifest)

  else:
    print 'No versions need auto-updating.'


if __name__ == '__main__':
  sys.exit(main(sys.argv))
