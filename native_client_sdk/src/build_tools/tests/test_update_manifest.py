#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import datetime
import os
import posixpath
import subprocess
import sys
import unittest
import urlparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(BUILD_TOOLS_DIR)
import manifest_util
import update_nacl_manifest


HTTPS_BASE_URL = 'https://commondatastorage.googleapis.com' \
    '/nativeclient_mirror/nacl/nacl_sdk/'

OS_CR = ('cros',)
OS_M = ('mac',)
OS_ML = ('mac', 'linux')
OS_MLW = ('mac', 'linux', 'win')
STABLE = 'stable'
BETA = 'beta'
DEV = 'dev'
CANARY = 'canary'


def GetArchiveUrl(host_os, version):
  basename = 'naclsdk_%s.bz2' % (host_os,)
  return urlparse.urljoin(HTTPS_BASE_URL, posixpath.join(version, basename))


def GetPathFromGsUrl(url):
  assert url.startswith(update_nacl_manifest.GS_BUCKET_PATH)
  return url[len(update_nacl_manifest.GS_BUCKET_PATH):]


def GetPathFromHttpsUrl(url):
  assert url.startswith(HTTPS_BASE_URL)
  return url[len(HTTPS_BASE_URL):]


def MakeArchive(host_os, version):
  archive = manifest_util.Archive(host_os)
  archive.url = GetArchiveUrl(host_os, version)
  # dummy values that won't succeed if we ever use them, but will pass
  # validation. :)
  archive.checksum = {'sha1': 'foobar'}
  archive.size = 1
  return archive


def MakeNonPepperBundle(name, with_archives=False):
  bundle = manifest_util.Bundle(name)
  bundle.version = 1
  bundle.revision = 1
  bundle.description = 'Dummy bundle'
  bundle.recommended = 'yes'
  bundle.stability = 'stable'

  if with_archives:
    for host_os in OS_MLW:
      archive = manifest_util.Archive(host_os)
      archive.url = 'http://example.com'
      archive.checksum = {'sha1': 'blah'}
      archive.size = 2
      bundle.AddArchive(archive)
  return bundle


def MakeBundle(major_version, revision, version=None, host_oses=None):
  assert version is None or version.split('.')[0] == major_version
  bundle_name = 'pepper_' + major_version
  bundle = manifest_util.Bundle(bundle_name)
  bundle.version = int(major_version)
  bundle.revision = revision
  bundle.description = 'Chrome %s bundle, revision %s' % (major_version,
      revision)
  bundle.repath = bundle_name
  bundle.recommended = 'no'
  bundle.stability = 'dev'

  if host_oses:
    for host_os in host_oses:
      bundle.AddArchive(MakeArchive(host_os, version))
  return bundle


class MakeManifest(manifest_util.SDKManifest):
  def __init__(self, *args):
    manifest_util.SDKManifest.__init__(self)

    for bundle in args:
      self.AddBundle(bundle)

  def AddBundle(self, bundle):
    self.MergeBundle(bundle, allow_existing=False)


class MakeHistory(object):
  def __init__(self):
    # used for a dummy timestamp
    self.datetime = datetime.datetime.utcnow()
    self.history = []

  def Add(self, host_oses, channel, version):
    for host_os in host_oses:
      timestamp = self.datetime.strftime('%Y-%m-%d %H:%M:%S.%f')
      self.history.append((host_os, channel, version, timestamp))
      self.datetime += datetime.timedelta(0, -3600) # one hour earlier
    self.datetime += datetime.timedelta(-1) # one day earlier


class MakeFiles(dict):
  def Add(self, bundle, add_archive_for_os=OS_MLW, add_json_for_os=OS_MLW):
    for archive in bundle.GetArchives():
      if not archive.host_os in add_archive_for_os:
        continue

      # add a dummy file for each archive
      path = GetPathFromHttpsUrl(archive.url)
      self[path] = 'My Dummy Archive'

      if archive.host_os in add_json_for_os:
        # add .json manifest snippet, it should look like a normal Bundle, but
        # only has one archive.
        new_bundle = manifest_util.Bundle('')
        new_bundle.CopyFrom(bundle)
        del new_bundle.archives[:]
        new_bundle.AddArchive(archive)
        self[path + '.json'] = new_bundle.GetDataAsString()


class TestDelegate(update_nacl_manifest.Delegate):
  def __init__(self, manifest, history, files):
    self.manifest = manifest
    self.history = history
    self.files = files

  def GetRepoManifest(self):
    return self.manifest

  def GetHistory(self):
    return self.history

  def GsUtil_ls(self, url):
    path = GetPathFromGsUrl(url)
    result = []
    for filename, _ in self.files.iteritems():
      if filename.startswith(path):
        result.append(filename)
    return result

  def GsUtil_cat(self, url):
    path = GetPathFromGsUrl(url)
    if path not in self.files:
      raise subprocess.CalledProcessError(1, 'gsutil cat %s' % (url,))
    return self.files[path]

  def GsUtil_cp(self, src, dest, stdin=None):
    dest_path = GetPathFromGsUrl(dest)
    if src == '-':
      self.files[dest_path] = stdin
    else:
      src_path = GetPathFromGsUrl(src)
      if src_path not in self.files:
        raise subprocess.CalledProcessError(1, 'gsutil cp %s %s' % (src, dest))
      self.files[dest_path] = self.files[src_path]

  def Print(self, *args):
    # eat all informational messages
    pass


# Shorthand for premade bundles/versions
V18_0_1025_163 = '18.0.1025.163'
V18_0_1025_175 = '18.0.1025.175'
V18_0_1025_184 = '18.0.1025.184'
V19_0_1084_41 = '19.0.1084.41'
V19_0_1084_67 = '19.0.1084.67'
B18_0_1025_163_R1_MLW = MakeBundle('18', '1', V18_0_1025_163, OS_MLW)
B18_0_1025_184_R1_MLW = MakeBundle('18', '1', V18_0_1025_184, OS_MLW)
B18_R1_NONE = MakeBundle('18', '1')
B19_0_1084_41_R1_MLW = MakeBundle('19', '1', V19_0_1084_41, OS_MLW)
B19_0_1084_67_R1_MLW = MakeBundle('19', '1', V19_0_1084_67, OS_MLW)
B19_R1_NONE = MakeBundle('19', '1')
NON_PEPPER_BUNDLE_NOARCHIVES = MakeNonPepperBundle('foo')
NON_PEPPER_BUNDLE_ARCHIVES = MakeNonPepperBundle('bar', with_archives=True)


class TestUpdateManifest(unittest.TestCase):
  def setUp(self):
    self.history = MakeHistory()
    self.files = MakeFiles()
    self.delegate = None
    self.uploaded_manifest = None
    self.manifest = None

  def _MakeDelegate(self):
    self.delegate = TestDelegate(self.manifest, self.history.history,
        self.files)

  def _Run(self, host_oses):
    update_nacl_manifest.Run(self.delegate, host_oses)

  def _HasUploadedManifest(self):
    return 'naclsdk_manifest2.json' in self.files

  def _ReadUploadedManifest(self):
    self.uploaded_manifest = manifest_util.SDKManifest()
    self.uploaded_manifest.LoadDataFromString(
        self.files['naclsdk_manifest2.json'])

  def _AssertUploadedManifestHasBundle(self, bundle, stability):
    uploaded_manifest_bundle = self.uploaded_manifest.GetBundle(bundle.name)
    # Bundles that we create in the test (and in the manifest snippets) have
    # their stability set to "dev". update_nacl_manifest correctly updates it.
    # So we have to force the stability of |bundle| so they compare equal.
    test_bundle = copy.copy(bundle)
    test_bundle.stability = stability
    self.assertEqual(uploaded_manifest_bundle, test_bundle)

  def testNoUpdateNeeded(self):
    self.manifest = MakeManifest(B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self.assertEqual(self._HasUploadedManifest(), False)

    # Add another bundle, make sure it still doesn't update
    self.manifest.AddBundle(B19_0_1084_41_R1_MLW)
    self._Run(OS_MLW)
    self.assertEqual(self._HasUploadedManifest(), False)

  def testSimpleUpdate(self):
    self.manifest = MakeManifest(B18_R1_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_R1_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testOnePlatformHasNewerRelease(self):
    self.manifest = MakeManifest(B18_R1_NONE)
    self.history.Add(OS_M, BETA, V18_0_1025_175)  # Mac has newer version
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_R1_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testMultipleMissingPlatformsInHistory(self):
    self.manifest = MakeManifest(B18_R1_NONE)
    self.history.Add(OS_ML, BETA, V18_0_1025_184)
    self.history.Add(OS_M, BETA, V18_0_1025_175)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_R1_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testUpdateOnlyOneBundle(self):
    self.manifest = MakeManifest(B18_R1_NONE, B19_0_1084_41_R1_MLW)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_R1_MLW, BETA)
    self._AssertUploadedManifestHasBundle(B19_0_1084_41_R1_MLW, DEV)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 2)

  def testUpdateTwoBundles(self):
    self.manifest = MakeManifest(B18_R1_NONE, B19_R1_NONE)
    self.history.Add(OS_MLW, BETA, V19_0_1084_41)
    self.history.Add(OS_MLW, STABLE, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_R1_MLW)
    self.files.Add(B19_0_1084_41_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_R1_MLW, STABLE)
    self._AssertUploadedManifestHasBundle(B19_0_1084_41_R1_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 2)

  def testUpdateWithMissingPlatformsInArchives(self):
    self.manifest = MakeManifest(B18_R1_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_184)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_184_R1_MLW, add_archive_for_os=OS_M)
    self.files.Add(B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_R1_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testUpdateWithMissingManifestSnippets(self):
    self.manifest = MakeManifest(B18_R1_NONE)
    self.history.Add(OS_MLW, BETA, V18_0_1025_184)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_184_R1_MLW, add_json_for_os=OS_ML)
    self.files.Add(B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_R1_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)

  def testRecommendedIsMaintained(self):
    for recommended in 'yes', 'no':
      self.setUp()
      bundle = copy.deepcopy(B18_R1_NONE)
      bundle.recommended = recommended
      self.manifest = MakeManifest(bundle)
      self.history.Add(OS_MLW, BETA, V18_0_1025_163)
      self.files.Add(B18_0_1025_163_R1_MLW)
      self._MakeDelegate()
      self._Run(OS_MLW)
      self._ReadUploadedManifest()
      self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)
      uploaded_bundle = self.uploaded_manifest.GetBundle('pepper_18')
      self.assertEqual(uploaded_bundle.recommended, recommended)

  def testNoUpdateWithNonPepperBundle(self):
    self.manifest = MakeManifest(NON_PEPPER_BUNDLE_NOARCHIVES,
        B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self.assertEqual(self._HasUploadedManifest(), False)

  def testUpdateWithHistoryWithExtraneousPlatforms(self):
    self.manifest = MakeManifest(B18_R1_NONE)
    self.history.Add(OS_ML, BETA, V18_0_1025_184)
    self.history.Add(OS_CR, BETA, V18_0_1025_184)
    self.history.Add(OS_CR, BETA, V18_0_1025_175)
    self.history.Add(OS_MLW, BETA, V18_0_1025_163)
    self.files.Add(B18_0_1025_163_R1_MLW)
    self._MakeDelegate()
    self._Run(OS_MLW)
    self._ReadUploadedManifest()
    self._AssertUploadedManifestHasBundle(B18_0_1025_163_R1_MLW, BETA)
    self.assertEqual(len(self.uploaded_manifest.GetBundles()), 1)


def main():
  suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(main())
