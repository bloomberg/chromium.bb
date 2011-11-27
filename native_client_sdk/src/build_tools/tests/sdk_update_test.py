#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for sdk_update.py."""

import exceptions
import mox
import os
import subprocess
import sys
import tempfile
import unittest
import urllib

from build_tools.sdk_tools import sdk_update


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)


class FakeOptions(object):
  ''' Just a placeholder for options '''
  pass


def CallSDKUpdate(args):
  '''Calls the sdk_update.py utility and returns stdout as a string

  Args:
    args: command-line arguments as a list (not including program name)

  Returns:
    string tuple containing (stdout, stderr)

  Raises:
    subprocess.CalledProcessError: non-zero return code from sdk_update'''
  command = ['python', os.path.join(PARENT_DIR, 'sdk_tools',
                                    'sdk_update.py')] + args
  process = subprocess.Popen(stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             args=command)
  output, error_output = process.communicate()

  retcode = process.poll() # Note - calling wait() can cause a deadlock
  if retcode != 0:
    raise subprocess.CalledProcessError(retcode, command)
  return output, error_output


class TestSDKUpdate(unittest.TestCase):
  ''' Test basic functionality of the sdk_update package '''
  def setUp(self):
    self._options = FakeOptions()
    self._options.manifest_url = 'file://%s' % urllib.pathname2url(
        os.path.join(SCRIPT_DIR, 'naclsdk_manifest_test.json'))
    self._options.user_data_dir = os.path.join(SCRIPT_DIR, 'sdk_test_cache')

  def testBadArg(self):
    '''Test that using a bad argument results in an error'''
    self.assertRaises(subprocess.CalledProcessError, CallSDKUpdate, ['--bad'])

  def testGetHostOS(self):
    '''Test that the GetHostOS function returns a valid value'''
    self.assertTrue(sdk_update.GetHostOS() in ['linux', 'mac', 'win'])

  def testHelp(self):
    '''Test that basic help works'''
    # Running any help command should call sys.exit()
    self.assertRaises(exceptions.SystemExit, sdk_update.main, ['-h'])

  def testList(self):
    '''Test the List function'''
    command = ['--user-data-dir=%s' %
                  os.path.join(SCRIPT_DIR, 'sdk_test_cache'),
               '--manifest-url=file://%s' %
                   urllib.pathname2url(os.path.join(
                       SCRIPT_DIR, 'naclsdk_manifest_test.json')),
               'list']
    bundle_list = CallSDKUpdate(command)[0]
    # Just do some simple sanity checks on the resulting string
    self.assertEqual(bundle_list.count('sdk_tools'), 2)
    self.assertEqual(bundle_list.count('test_1'), 2)
    self.assertEqual(bundle_list.count('test_2'), 1)
    self.assertEqual(bundle_list.count('description:'), 6)

  def testUpdateHelp(self):
    '''Test the help for the update command'''
    self.assertRaises(exceptions.SystemExit,
                      sdk_update.main, ['help', 'update'])

  def testUpdateBogusBundle(self):
    '''Test running update with a bogus bundle'''
    self.assertRaises(sdk_update.Error,
                      sdk_update.main,
                      ['update', 'bogusbundle'])

  def testSDKManifestFile(self):
    '''Test SDKManifestFile'''
    manifest_file = sdk_update.SDKManifestFile(
        os.path.join(self._options.user_data_dir,
                     sdk_update.MANIFEST_FILENAME))
    self.assertNotEqual(None, manifest_file)
    bundles = manifest_file.GetBundles()
    self.assertEqual(3, len(bundles))
    test_bundle = manifest_file.GetBundleNamed('test_1')
    self.assertNotEqual(None, test_bundle)
    self.assertTrue('revision' in test_bundle)
    self.assertEqual(1, test_bundle['revision'])

  def testNeedsUpdate(self):
    '''Test that the test_1 bundle needs updating'''
    tools = sdk_update.ManifestTools(self._options)
    tools.LoadManifest()
    bundles = tools.GetBundles()
    self.assertEqual(3, len(bundles))
    local_manifest = sdk_update.SDKManifestFile(
        os.path.join(self._options.user_data_dir,
                     sdk_update.MANIFEST_FILENAME))
    self.assertNotEqual(None, local_manifest)
    for bundle in bundles:
      bundle_name = bundle['name']
      self.assertTrue('revision' in bundle)
      if bundle_name == 'test_1':
        self.assertTrue(local_manifest.BundleNeedsUpdate(bundle))
      elif bundle_name == 'test_2':
        self.assertTrue(local_manifest.BundleNeedsUpdate(bundle))
      else:
        self.assertFalse(local_manifest.BundleNeedsUpdate(bundle))

  def testMergeManifests(self):
    '''Test merging a Bundle into a manifest file'''
    tools = sdk_update.ManifestTools(self._options)
    tools.LoadManifest()
    bundles = tools.GetBundles()
    self.assertEqual(3, len(bundles))
    local_manifest = sdk_update.SDKManifestFile(
        os.path.join(self._options.user_data_dir,
                     sdk_update.MANIFEST_FILENAME))
    self.assertEqual(None, local_manifest.GetBundleNamed('test_2'))
    for bundle in bundles:
      local_manifest.MergeBundle(bundle)
    self.assertNotEqual(None, local_manifest.GetBundleNamed('test_2'))
    for bundle in bundles:
      self.assertFalse(local_manifest.BundleNeedsUpdate(bundle))

  def testMergeBundle(self):
    '''Test MergeWithBundle'''
    tools = sdk_update.ManifestTools(self._options)
    tools.LoadManifest()
    bundles = tools.GetBundles()
    self.assertEqual(3, len(bundles))
    local_manifest = sdk_update.SDKManifestFile(
        os.path.join(self._options.user_data_dir,
                     sdk_update.MANIFEST_FILENAME))
    self.assertNotEqual(None, local_manifest)
    # Test the | operator.
    for bundle in bundles:
      bundle_name = bundle['name']
      if bundle_name == 'test_2':
        continue
      local_test_bundle = local_manifest.GetBundleNamed(bundle_name)
      merged_bundle = local_test_bundle.MergeWithBundle(bundle)
      self.assertTrue('revision' in merged_bundle)
      if bundle_name == 'test_1':
        self.assertEqual(2, merged_bundle['revision'])
      else:
        self.assertEqual(1, merged_bundle['revision'])
      merged_bundle.Validate()

  def testVersion(self):
    '''Test that showing the version works'''
    self.assertRaises(exceptions.SystemExit, sdk_update.main, ['--version'])


def main():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestSDKUpdate)
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())


if __name__ == '__main__':
  sys.exit(main())
