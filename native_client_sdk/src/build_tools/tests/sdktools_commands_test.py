#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import re
import tarfile
import tempfile
import unittest
from sdktools_test import SdkToolsTestCase

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
TOOLS_DIR = os.path.join(os.path.dirname(BUILD_TOOLS_DIR), 'tools')

sys.path.extend([BUILD_TOOLS_DIR, TOOLS_DIR])
import manifest_util
import oshelpers


class TestCommands(SdkToolsTestCase):
  def setUp(self):
    self.SetupDefault()

  def _AddDummyBundle(self, manifest, bundle_name):
    bundle = manifest_util.Bundle(bundle_name)
    bundle.revision = 1337
    bundle.version = 23
    bundle.description = bundle_name
    bundle.stability = 'beta'
    bundle.recommended = 'no'
    bundle.repath = bundle_name
    archive = self._MakeDummyArchive(bundle_name)
    bundle.AddArchive(archive)
    manifest.SetBundle(bundle)

    # Need to get the bundle from the manifest -- it doesn't use the one we
    # gave it.
    return manifest.GetBundle(bundle_name)

  def _MakeDummyArchive(self, bundle_name):
    temp_dir = tempfile.mkdtemp(prefix='archive')
    try:
      dummy_path = os.path.join(temp_dir, 'dummy.txt')
      with open(dummy_path, 'w') as stream:
        stream.write('Dummy stuff for %s' % (bundle_name,))

      # Build the tarfile directly into the server's directory.
      tar_path = os.path.join(self.basedir, bundle_name + '.tar.bz2')
      tarstream = tarfile.open(tar_path, 'w:bz2')
      try:
        tarstream.add(dummy_path, os.path.join(bundle_name, 'dummy.txt'))
      finally:
        tarstream.close()

      with open(tar_path, 'rb') as archive_stream:
        sha1, size = manifest_util.DownloadAndComputeHash(archive_stream)

      archive = manifest_util.Archive(manifest_util.GetHostOS())
      archive.url = self.server.GetURL(os.path.basename(tar_path))
      archive.size = size
      archive.checksum = sha1
      return archive
    finally:
      oshelpers.Remove(['-rf', temp_dir])

  def testInfoBasic(self):
    """The info command should display information about the given bundle."""
    self._WriteManifest()
    output = self._Run(['info', 'sdk_tools'])
    # Make sure basic information is there
    bundle = self.manifest.GetBundle('sdk_tools')
    archive = bundle.GetHostOSArchive();
    self.assertTrue(bundle.name in output)
    self.assertTrue(bundle.description in output)
    self.assertTrue(str(bundle.revision) in output)
    self.assertTrue(str(archive.size) in output)
    self.assertTrue(archive.checksum in output)
    self.assertTrue(bundle.stability in output)

  def testInfoUnknownBundle(self):
    """The info command should notify the user of unknown bundles."""
    self._WriteManifest()
    bogus_bundle = 'foobar'
    output = self._Run(['info', bogus_bundle])
    self.assertTrue(re.search(r'[uU]nknown', output))
    self.assertTrue(bogus_bundle in output)

  def testInfoMultipleBundles(self):
    """The info command should support listing multiple bundles."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._AddDummyBundle(self.manifest, 'pepper_24')
    self._WriteManifest()
    output = self._Run(['info', 'pepper_23', 'pepper_24'])
    self.assertTrue('pepper_23' in output)
    self.assertTrue('pepper_24' in output)
    self.assertFalse(re.search(r'[uU]nknown', output))

  def testListBasic(self):
    """The list command should display basic information about remote
    bundles."""
    self._WriteManifest()
    output = self._Run(['list'])
    self.assertTrue(re.search('I.*?sdk_tools.*?stable', output, re.MULTILINE))
    # This line is important (it's used by the updater to determine if the
    # sdk_tools bundle needs to be updated), so let's be explicit.
    self.assertTrue('All installed bundles are up-to-date.')

  def testListMultiple(self):
    """The list command should display multiple bundles."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    output = self._Run(['list'])
    # Added pepper_23 to the remote manifest not the local manifest, so it
    # shouldn't be installed.
    self.assertTrue(re.search('^[^I]*pepper_23', output, re.MULTILINE))
    self.assertTrue('sdk_tools' in output)

  def testListWithRevision(self):
    """The list command should display the revision, if desired."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    output = self._Run(['list', '-r'])
    self.assertTrue(re.search('pepper_23.*?r1337', output))

  def testListWithUpdatedRevision(self):
    """The list command should display when there is an update available."""
    p23bundle = self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteCacheManifest(self.manifest)
    # Modify the remote manifest to have a newer revision.
    p23bundle.revision += 1
    self._WriteManifest()
    output = self._Run(['list', '-r'])
    # We should see a display like this:  I* pepper_23 (r1337 -> r1338)
    # The star indicates the bundle has an update.
    self.assertTrue(re.search('I\*\s+pepper_23.*?r1337.*?r1338', output))

  def testListLocalVersionNotOnRemote(self):
    """The list command should tell the user if they have a bundle installed
    that doesn't exist in the remote manifest."""
    self._WriteManifest()
    p23bundle = self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteCacheManifest(self.manifest)
    output = self._Run(['list', '-r'])
    message = 'Bundles installed locally that are not available remotely:'
    message_loc = output.find(message)
    self.assertNotEqual(message_loc, -1)
    # Make sure pepper_23 is listed after the message above.
    self.assertTrue('pepper_23' in output[message_loc:])

  def testSources(self):
    """The sources command should allow adding/listing/removing of sources.
    When a source is added, it will provide an additional set of bundles."""
    other_manifest = manifest_util.SDKManifest()
    self._AddDummyBundle(other_manifest, 'naclmono_23')
    with open(os.path.join(self.basedir, 'source.json'), 'w') as stream:
      stream.write(other_manifest.GetDataAsString())

    source_json_url = self.server.GetURL('source.json')
    self._WriteManifest()
    output = self._Run(['sources', '--list'])
    self.assertTrue('No external sources installed.' in output)
    output = self._Run(['sources', '--add', source_json_url])
    output = self._Run(['sources', '--list'])
    self.assertTrue(source_json_url in output)

    # Should be able to get info about that bundle.
    output = self._Run(['info', 'naclmono_23'])
    self.assertTrue('Unknown bundle' not in output)

    self._Run(['sources', '--remove', source_json_url])
    output = self._Run(['sources', '--list'])
    self.assertTrue('No external sources installed.' in output)

  def testUpdateBasic(self):
    """The update command should install the contents of a bundle to the SDK."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    output = self._Run(['update', 'pepper_23'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy.txt')))

  def testUpdateInCacheButDirectoryRemoved(self):
    """The update command should update if the bundle directory does not exist,
    even if the bundle is already in the cache manifest."""
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteCacheManifest(self.manifest)
    self._WriteManifest()
    output = self._Run(['update', 'pepper_23'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_23', 'dummy.txt')))

  def testUpdateNoNewVersion(self):
    """The update command should do nothing if the bundle is already up-to-date.
    """
    self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])
    output = self._Run(['update', 'pepper_23'])
    self.assertTrue('is already up-to-date.' in output)

  def testUpdateWithNewVersion(self):
    """The update command should update to a new version if it exists."""
    bundle = self._AddDummyBundle(self.manifest, 'pepper_23')
    self._WriteManifest()
    self._Run(['update', 'pepper_23'])

    bundle.revision += 1
    self._WriteManifest()
    output = self._Run(['update', 'pepper_23'])
    self.assertTrue('already exists, but has an update available' in output)

    # Now update using --force.
    output = self._Run(['update', 'pepper_23', '--force'])
    self.assertTrue('Updating bundle' in output)

  def testUpdateUnknownBundles(self):
    """The update command should ignore unknown bundles and notify the user."""
    self._WriteManifest()
    output = self._Run(['update', 'foobar'])
    self.assertTrue('unknown bundle' in output)

  def testUpdateRecommended(self):
    """The update command should update only recommended bundles when run
    without args.
    """
    bundle = self._AddDummyBundle(self.manifest, 'pepper_26')
    bundle.recommended = 'yes'
    self._WriteManifest()
    output = self._Run(['update'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_26', 'dummy.txt')))

  def testUpdateCanary(self):
    """The update command should create the correct directory name for repath'd
    bundles.
    """
    bundle = self._AddDummyBundle(self.manifest, 'pepper_26')
    bundle.name = 'pepper_canary'
    self._WriteManifest()
    output = self._Run(['update'])
    self.assertTrue(os.path.exists(
        os.path.join(self.basedir, 'nacl_sdk', 'pepper_canary', 'dummy.txt')))


if __name__ == '__main__':
  unittest.main()
