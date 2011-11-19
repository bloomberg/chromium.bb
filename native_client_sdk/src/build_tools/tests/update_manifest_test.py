#!/usr/bin/python2.6
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for update_manifest.py."""

__author__ = 'mball@google.com (Matt Ball)'

import errno
import os
import SimpleHTTPServer
import SocketServer
import sys
import tempfile
import threading
import unittest
import urlparse

from build_tools.sdk_tools import sdk_update
from build_tools.sdk_tools import update_manifest

TEST_DIR = os.path.dirname(os.path.abspath(__file__))


def RemoveFile(filename):
  '''Remove a filename if it exists and do nothing if it doesn't exist'''
  try:
    os.remove(filename)
  except OSError as error:
    if error.errno != errno.ENOENT:
      raise


def GetHTTPHandler(path, length=None):
  '''Returns a simple HTTP Request Handler that only servers up a given file

  Args:
    path: path and filename of the file to serve up
    length: (optional) only serve up the first |length| bytes

  Returns:
    A SimpleHTTPRequestHandler class'''
  class HTTPHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def do_GET(self):
      with open(path, 'rb') as f:
        # This code is largely lifted from SimpleHTTPRequestHandler.send_head
        self.send_response(200)
        self.send_header("Content-type", self.guess_type(path))
        fs = os.fstat(f.fileno())
        self.send_header("Content-Length", str(fs[6]))
        self.send_header("Last-Modified", self.date_time_string(fs.st_mtime))
        self.end_headers()
        if length != None:
          self.wfile.write(f.read(length))
        else:
          self.copyfile(f, self.wfile)
  return HTTPHandler


class FakeOptions(object):
  ''' Just a place holder for options '''
  def __init__(self):
    self.bundle_desc_url = None
    self.bundle_name = None
    self.bundle_version = None
    self.bundle_revision = None
    self.desc = None
    self.gsutil = os.path.join(TEST_DIR, 'fake_gsutil.bat'
                               if sys.platform == 'win32' else 'fake_gsutil.py')
    self.linux_arch_url = None
    self.mac_arch_url = None
    self.manifest_file = os.path.join(TEST_DIR, 'naclsdk_manifest_test.json')
    self.manifest_version = None
    self.recommended = None
    self.root_url = 'http://localhost/test_url'
    self.stability = None
    self.upload = False
    self.win_arch_url = None


class TestUpdateManifest(unittest.TestCase):
  ''' Test basic functionality of the update_manifest package.

  Note that update_manifest.py now imports sdk_update.py, so this file
  tests the update_manifest features within sdk_update.'''

  def setUp(self):
    self._json_boilerplate=(
        '{\n'
        '  "bundles": [],\n'
        '  "manifest_version": 1\n'
        '}\n')
    self._temp_dir = tempfile.gettempdir()
    # os.path.join('build_tools', 'tests', 'test_archive')
    self._manifest = update_manifest.UpdateSDKManifest()

  def testJSONBoilerplate(self):
    ''' Test creating a manifest object'''
    self.assertEqual(self._manifest.GetManifestString(),
                     self._json_boilerplate)
    # Test using a manifest file with a version that is too high
    self.assertRaises(sdk_update.Error,
                      self._manifest.LoadManifestString,
                      '{"manifest_version": 2}')

  def testWriteLoadManifestFile(self):
    ''' Test writing to and loading from a manifest file'''
    # Remove old test file
    file_path = os.path.join(self._temp_dir, 'temp_manifest.json')
    if os.path.exists(file_path):
      os.remove(file_path)
    # Create a basic manifest file
    manifest_file = sdk_update.SDKManifestFile(file_path)
    manifest_file.WriteFile();
    self.assertTrue(os.path.exists(file_path))
    # Test re-loading the file
    manifest_file._manifest._manifest_data['manifest_version'] = 0
    manifest_file._LoadFile()
    self.assertEqual(manifest_file._manifest.GetManifestString(),
                     self._json_boilerplate)
    os.remove(file_path)

  def testValidateBundleName(self):
    ''' Test validating good and bad bundle names '''
    self.assertTrue(
        self._manifest._ValidateBundleName('A_Valid.Bundle-Name(1)'))
    self.assertFalse(self._manifest._ValidateBundleName('A bad name'))
    self.assertFalse(self._manifest._ValidateBundleName('A bad/name'))
    self.assertFalse(self._manifest._ValidateBundleName('A bad;name'))
    self.assertFalse(self._manifest._ValidateBundleName('A bad,name'))

  def testUpdateManifestVersion(self):
    ''' Test updating the manifest version number '''
    options = FakeOptions()
    options.manifest_version = 99
    self.assertEqual(self._manifest._manifest_data['manifest_version'], 1)
    self._manifest._UpdateManifestVersion(options)
    self.assertEqual(self._manifest._manifest_data['manifest_version'], 99)

  def testVerifyAllOptionsConsumed(self):
    ''' Test function _VerifyAllOptionsConsumed '''
    options = FakeOptions()
    options.opt1 = None
    self.assertTrue(self._manifest._VerifyAllOptionsConsumed(options, None))
    options.opt2 = 'blah'
    self.assertRaises(update_manifest.Error,
                      self._manifest._VerifyAllOptionsConsumed,
                      options,
                      'no bundle name')

  def testBundleUpdate(self):
    ''' Test function Bundle.Update '''
    bundle = sdk_update.Bundle('test')
    options = FakeOptions()
    options.bundle_revision = 1
    options.bundle_version = 2
    options.desc = 'What a hoot'
    options.stability = 'dev'
    options.recommended = 'yes'
    update_manifest.UpdateBundle(bundle, options)
    self.assertEqual(bundle['revision'], 1)

  def testUpdateManifestModifyTopLevel(self):
    ''' Test function UpdateManifest: modifying top-level info '''
    options = FakeOptions()
    options.manifest_version = 0
    options.bundle_name = None
    self._manifest.UpdateManifest(options)
    self.assertEqual(self._manifest._manifest_data['manifest_version'], 0)

  def testUpdateManifestModifyBundle(self):
    ''' Test function UpdateManifest: adding/modifying a bundle '''
    # Add a bundle
    options = FakeOptions()
    options.manifest_version = 1
    options.bundle_name = 'test'
    options.bundle_revision = 2
    options.bundle_version = 3
    options.desc = 'nice bundle'
    options.stability = 'canary'
    options.recommended = 'yes'
    self._manifest.UpdateManifest(options)
    bundle = self._manifest.GetBundle('test')
    self.assertNotEqual(bundle, None)
    # Modify the same bundle
    options = FakeOptions()
    options.manifest_version = None
    options.bundle_name = 'test'
    options.desc = 'changed'
    self._manifest.UpdateManifest(options)
    bundle = self._manifest.GetBundle('test')
    self.assertEqual(bundle['description'], 'changed')

  def testUpdateManifestBadBundle1(self):
    ''' Test function UpdateManifest: bad bundle data '''
    options = FakeOptions()
    options.manifest_version = None
    options.bundle_name = 'test'
    options.stability = 'excellent'
    self.assertRaises(sdk_update.Error,
                      self._manifest.UpdateManifest,
                      options)

  def testUpdateManifestBadBundle2(self):
    ''' Test function UpdateManifest: incomplete bundle data '''
    options = FakeOptions()
    options.manifest_version = None
    options.bundle_name = 'another_bundle'
    self.assertRaises(sdk_update.Error,
                      self._manifest.UpdateManifest,
                      options)

  def testUpdateManifestArchiveComputeSha1AndSize(self):
    ''' Test function Archive.Update '''
    temp_file_path = None
    try:
      with tempfile.NamedTemporaryFile(delete=False) as temp_file:
        # Create a temp file with some data
        temp_file.write(r'abcdefghijklmnopqrstuvwxyz0123456789')
        temp_file_path = temp_file.name
        # Windows requires that we close the file before reading from it.
        temp_file.close()

        # Create an archive with a url to the file we created above.
        url_parts = urlparse.ParseResult('file', '', temp_file_path, '', '', '')
        url = urlparse.urlunparse(url_parts)
        archive = sdk_update.Archive('mac')
        archive.Update(url)
        self.assertEqual(archive['checksum']['sha1'],
                         'd2985049a677bbc4b4e8dea3b89c4820e5668e3a')
    finally:
      if temp_file_path and os.path.exists(temp_file_path):
        os.remove(temp_file_path)

  def testUpdateManifestArchiveValidate(self):
    ''' Test function Archive.Validate '''
    # Test invalid host-os name
    archive = sdk_update.Archive('atari')
    self.assertRaises(sdk_update.Error, archive.Validate)
    # Test missing url
    archive['host_os'] = 'mac'
    self.assertRaises(sdk_update.Error, archive.Validate)
    # Valid archive
    archive['url'] = 'http://www.google.com'
    archive.Validate()
    # Test invalid key name
    archive['guess'] = 'who'
    self.assertRaises(sdk_update.Error, archive.Validate)

  def testUpdatePartialFile(self):
    '''Test updating with a partially downloaded file'''
    server = None
    server_thread = None
    temp_filename = os.path.join(self._temp_dir,
                                 'testUpdatePartialFile_temp.txt')
    try:
      # Create a new local server on an arbitrary port that just serves-up
      # the first 10 bytes of this file.
      server = SocketServer.TCPServer(
          ("", 0), GetHTTPHandler(__file__, 10))
      ip, port = server.server_address
      server_thread = threading.Thread(target=server.serve_forever)
      server_thread.start()

      archive = sdk_update.Archive('mac')
      self.assertRaises(sdk_update.Error,
                        archive.Update,
                        'http://localhost:%s' % port)
      try:
        self.assertRaises(sdk_update.Error,
                          archive.DownloadToFile,
                          temp_filename)
      finally:
        RemoveFile(temp_filename)
    finally:
      if server_thread and server_thread.isAlive():
        server.shutdown()
        server_thread.join()

  def testUpdateManifestMain(self):
    ''' test the main function from update_manifest '''
    temp_filename = os.path.join(self._temp_dir, 'testUpdateManifestMain.json')
    try:
      argv = ['--bundle-version', '0',
              '--bundle-revision', '0',
              '--description', 'test bundle for update_manifest unit tests',
              '--bundle-name', 'test_bundle',
              '--stability', 'dev',
              '--recommended', 'no',
              '--manifest-file', temp_filename]
      update_manifest.main(argv)
    finally:
      RemoveFile(temp_filename)

  def testHandleSDKTools(self):
    '''Test the handling of the sdk_tools bundle'''
    options = FakeOptions()
    options.bundle_name = 'sdk_tools'
    options.upload = True
    options.bundle_version = 0
    self.assertRaises(
        update_manifest.Error,
        update_manifest.UpdateSDKManifestFile(options).HandleBundles)
    options.bundle_version = None
    options.bundle_revision = 0
    self.assertRaises(
        update_manifest.Error,
        update_manifest.UpdateSDKManifestFile(options).HandleBundles)
    options.bundle_revision = None
    update_manifest.UpdateSDKManifestFile(options).HandleBundles()

  def testHandlePepper(self):
    '''Test the handling of pepper bundles'''
    options = FakeOptions()
    options.bundle_name = 'pepper'
    options.bundle_version = None
    self.assertRaises(
        update_manifest.Error,
        update_manifest.UpdateSDKManifestFile(options).HandleBundles)
    options.bundle_version = 1
    options.bundle_revision = None
    self.assertRaises(
        update_manifest.Error,
        update_manifest.UpdateSDKManifestFile(options).HandleBundles)
    options.bundle_revision = 0
    update_manifest.UpdateSDKManifestFile(options).HandleBundles()
    options.bundle_name = 'pepper_1'
    options.bundle_version = None
    update_manifest.UpdateSDKManifestFile(options).HandleBundles()


def main():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestUpdateManifest)
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(main())
