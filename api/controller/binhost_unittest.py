# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Binhost operations."""

from __future__ import print_function

import os
import mock

from chromite.api import api_config
from chromite.api.controller import binhost
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.service import binhost as binhost_service


class GetBinhostsTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Unittests for GetBinhosts."""

  def setUp(self):
    self.response = binhost_pb2.BinhostGetResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service, 'GetBinhosts')

    request = binhost_pb2.BinhostGetRequest()
    request.build_target.name = 'target'
    binhost.GetBinhosts(request, self.response, self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(binhost_service, 'GetBinhosts')

    input_proto = binhost_pb2.BinhostGetRequest()
    input_proto.build_target.name = 'target'

    binhost.GetBinhosts(input_proto, self.response, self.mock_call_config)

    self.assertEqual(len(self.response.binhosts), 1)
    self.assertEqual(self.response.binhosts[0].package_index, 'Packages')
    patch.assert_not_called()

  def testGetBinhosts(self):
    """GetBinhosts calls service with correct args."""
    binhost_list = [
        'gs://cr-prebuilt/board/amd64-generic/paladin-R66-17.0.0-rc2/packages/',
        'gs://cr-prebuilt/board/eve/paladin-R66-17.0.0-rc2/packages/']
    get_binhost = self.PatchObject(binhost_service, 'GetBinhosts',
                                   return_value=binhost_list)

    input_proto = binhost_pb2.BinhostGetRequest()
    input_proto.build_target.name = 'target'

    binhost.GetBinhosts(input_proto, self.response, self.api_config)

    self.assertEqual(len(self.response.binhosts), 2)
    self.assertEqual(self.response.binhosts[0].package_index, 'Packages')
    get_binhost.assert_called_once_with(mock.ANY)


class GetPrivatePrebuiltAclArgsTest(cros_test_lib.MockTestCase,
                                    api_config.ApiConfigMixin):
  """Unittests for GetPrivatePrebuiltAclArgs."""

  def setUp(self):
    self.response = binhost_pb2.AclArgsResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service, 'GetPrebuiltAclArgs')

    request = binhost_pb2.AclArgsRequest()
    request.build_target.name = 'target'
    binhost.GetPrivatePrebuiltAclArgs(request, self.response,
                                      self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(binhost_service, 'GetPrebuiltAclArgs')

    input_proto = binhost_pb2.AclArgsRequest()
    input_proto.build_target.name = 'target'

    binhost.GetPrivatePrebuiltAclArgs(input_proto, self.response,
                                      self.mock_call_config)

    self.assertEqual(len(self.response.args), 1)
    self.assertEqual(self.response.args[0].arg, '-g')
    self.assertEqual(self.response.args[0].value, 'group1:READ')
    patch.assert_not_called()

  def testGetPrivatePrebuiltAclArgs(self):
    """GetPrivatePrebuildAclsArgs calls service with correct args."""
    argvalue_list = [['-g', 'group1:READ']]
    get_binhost = self.PatchObject(binhost_service, 'GetPrebuiltAclArgs',
                                   return_value=argvalue_list)

    input_proto = binhost_pb2.AclArgsRequest()
    input_proto.build_target.name = 'target'

    binhost.GetPrivatePrebuiltAclArgs(input_proto, self.response,
                                      self.api_config)

    self.assertEqual(len(self.response.args), 1)
    self.assertEqual(self.response.args[0].arg, '-g')
    self.assertEqual(self.response.args[0].value, 'group1:READ')
    get_binhost.assert_called_once_with(mock.ANY)


class PrepareBinhostUploadsTest(cros_test_lib.MockTestCase,
                                api_config.ApiConfigMixin):
  """Unittests for PrepareBinhostUploads."""

  def setUp(self):
    self.PatchObject(binhost_service, 'GetPrebuiltsRoot',
                     return_value='/build/target/packages')
    self.PatchObject(binhost_service, 'GetPrebuiltsFiles',
                     return_value=['foo.tbz2', 'bar.tbz2'])
    self.PatchObject(binhost_service, 'UpdatePackageIndex',
                     return_value='/build/target/packages/Packages')

    self.response = binhost_pb2.PrepareBinhostUploadsResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service, 'GetPrebuiltsRoot')

    request = binhost_pb2.PrepareBinhostUploadsRequest()
    request.build_target.name = 'target'
    request.uri = 'gs://chromeos-prebuilt/target'
    rc = binhost.PrepareBinhostUploads(request, self.response,
                                       self.validate_only_config)
    patch.assert_not_called()
    self.assertEqual(rc, 0)

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(binhost_service, 'GetPrebuiltsRoot')

    request = binhost_pb2.PrepareBinhostUploadsRequest()
    request.build_target.name = 'target'
    request.uri = 'gs://chromeos-prebuilt/target'
    rc = binhost.PrepareBinhostUploads(request, self.response,
                                       self.mock_call_config)
    self.assertEqual(self.response.uploads_dir, '/upload/directory')
    self.assertEqual(self.response.upload_targets[0].path, 'upload_target')
    patch.assert_not_called()
    self.assertEqual(rc, 0)

  def testPrepareBinhostUploads(self):
    """PrepareBinhostUploads returns Packages and tar files."""
    input_proto = binhost_pb2.PrepareBinhostUploadsRequest()
    input_proto.build_target.name = 'target'
    input_proto.uri = 'gs://chromeos-prebuilt/target'
    binhost.PrepareBinhostUploads(input_proto, self.response, self.api_config)
    self.assertEqual(self.response.uploads_dir, '/build/target/packages')
    self.assertCountEqual(
        [ut.path for ut in self.response.upload_targets],
        ['Packages', 'foo.tbz2', 'bar.tbz2'])

  def testPrepareBinhostUploadsNonGsUri(self):
    """PrepareBinhostUploads dies when URI does not point to GS."""
    input_proto = binhost_pb2.PrepareBinhostUploadsRequest()
    input_proto.build_target.name = 'target'
    input_proto.uri = 'https://foo.bar'
    with self.assertRaises(ValueError):
      binhost.PrepareBinhostUploads(input_proto, self.response, self.api_config)


class SetBinhostTest(cros_test_lib.MockTestCase, api_config.ApiConfigMixin):
  """Unittests for SetBinhost."""

  def setUp(self):
    self.response = binhost_pb2.SetBinhostResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service, 'SetBinhost')

    request = binhost_pb2.SetBinhostRequest()
    request.build_target.name = 'target'
    request.key = binhost_pb2.POSTSUBMIT_BINHOST
    request.uri = 'gs://chromeos-prebuilt/target'
    binhost.SetBinhost(request, self.response, self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(binhost_service, 'SetBinhost')

    request = binhost_pb2.SetBinhostRequest()
    request.build_target.name = 'target'
    request.key = binhost_pb2.POSTSUBMIT_BINHOST
    request.uri = 'gs://chromeos-prebuilt/target'
    binhost.SetBinhost(request, self.response, self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(self.response.output_file, '/path/to/BINHOST.conf')

  def testSetBinhost(self):
    """SetBinhost calls service with correct args."""
    set_binhost = self.PatchObject(binhost_service, 'SetBinhost',
                                   return_value='/path/to/BINHOST.conf')

    input_proto = binhost_pb2.SetBinhostRequest()
    input_proto.build_target.name = 'target'
    input_proto.private = True
    input_proto.key = binhost_pb2.POSTSUBMIT_BINHOST
    input_proto.uri = 'gs://chromeos-prebuilt/target'

    binhost.SetBinhost(input_proto, self.response, self.api_config)

    self.assertEqual(self.response.output_file, '/path/to/BINHOST.conf')
    set_binhost.assert_called_once_with(
        'target',
        'POSTSUBMIT_BINHOST',
        'gs://chromeos-prebuilt/target',
        private=True)


class RegenBuildCacheTest(cros_test_lib.MockTestCase,
                          api_config.ApiConfigMixin):
  """Unittests for RegenBuildCache."""

  def setUp(self):
    self.response = binhost_pb2.RegenBuildCacheResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service, 'RegenBuildCache')

    request = binhost_pb2.RegenBuildCacheRequest()
    request.overlay_type = binhost_pb2.OVERLAYTYPE_BOTH
    binhost.RegenBuildCache(request, self.response, self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(binhost_service, 'RegenBuildCache')

    request = binhost_pb2.RegenBuildCacheRequest()
    request.overlay_type = binhost_pb2.OVERLAYTYPE_BOTH
    binhost.RegenBuildCache(request, self.response, self.mock_call_config)
    patch.assert_not_called()
    self.assertEqual(len(self.response.modified_overlays), 1)
    self.assertEqual(self.response.modified_overlays[0].path,
                     '/path/to/BuildCache')


  def testRegenBuildCache(self):
    """RegenBuildCache calls service with the correct args."""
    regen_cache = self.PatchObject(binhost_service, 'RegenBuildCache')

    input_proto = binhost_pb2.RegenBuildCacheRequest()
    input_proto.overlay_type = binhost_pb2.OVERLAYTYPE_BOTH

    binhost.RegenBuildCache(input_proto, self.response, self.api_config)
    regen_cache.assert_called_once_with(mock.ANY, 'both')

  def testRequiresOverlayType(self):
    """RegenBuildCache dies if overlay_type not specified."""
    regen_cache = self.PatchObject(binhost_service, 'RegenBuildCache')

    input_proto = binhost_pb2.RegenBuildCacheRequest()
    input_proto.overlay_type = binhost_pb2.OVERLAYTYPE_UNSPECIFIED

    with self.assertRaises(cros_build_lib.DieSystemExit):
      binhost.RegenBuildCache(input_proto, self.response, self.api_config)
    regen_cache.assert_not_called()


class PrepareDevInstallerBinhostUploadsTest(cros_test_lib.MockTempDirTestCase,
                                            api_config.ApiConfigMixin):
  """Tests for the UploadDevInstallerPrebuilts stage."""
  def setUp(self):
    # target packages dir
    self.chroot_path = os.path.join(self.tempdir, 'chroot')
    self.sysroot_path = '/build/target'
    self.chroot_path = os.path.join(self.tempdir, 'chroot')
    full_sysroot_path = os.path.join(self.chroot_path,
                                     self.sysroot_path.lstrip(os.sep))
    self.full_sysroot_package_path = os.path.join(full_sysroot_path,
                                                  'packages')
    osutils.SafeMakedirs(self.full_sysroot_package_path)
    self.uploads_dir = os.path.join(self.tempdir, 'uploads_dir')
    osutils.SafeMakedirs(self.uploads_dir)
    # Create packages/Packages file
    packages_file = os.path.join(self.full_sysroot_package_path, 'Packages')
    packages_content = """\
USE: test

CPV: app-arch/brotli-1.0.6

CPV: app-arch/zip-3.0-r3

CPV: chromeos-base/shill-0.0.1-r1

CPV: chromeos-base/test-0.0.1-r1

CPV: virtual/chromium-os-printing-1-r4

CPV: virtual/python-enum34-1

"""
    osutils.WriteFile(packages_file, packages_content)


    # Create package.installable file
    self.dev_install_packages = ['app-arch/zip-3.0-r3',
                                 'virtual/chromium-os-printing-1-r4',
                                 'virtual/python-enum34-1']
    package_installable_dir = os.path.join(full_sysroot_path,
                                           'build/dev-install')
    osutils.SafeMakedirs(package_installable_dir)
    package_installable_filename = os.path.join(package_installable_dir,
                                                'package.installable')

    # Create path to the dev_install_packages
    packages_dir = os.path.join(full_sysroot_path, 'packages')
    osutils.SafeMakedirs(packages_dir)
    for package in self.dev_install_packages:
      # Since a package has a category, such as app-arch/zip-3.0-r3, we need
      # to create the packages_dir / category dir as needed.
      category = package.split(os.sep)[0]
      osutils.SafeMakedirs(os.path.join(packages_dir, category))
      package_tbz2_file = os.path.join(packages_dir, package) + '.tbz2'
      osutils.Touch(package_tbz2_file)
    package_installable_file = open(package_installable_filename, 'w')
    for package in self.dev_install_packages:
      package_installable_file.write(package + '\n')
    package_installable_file.close()
    self.response = binhost_pb2.PrepareDevInstallBinhostUploadsResponse()

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(binhost_service,
                             'ReadDevInstallFilesToCreatePackageIndex')

    input_proto = binhost_pb2.PrepareDevInstallBinhostUploadsRequest()
    input_proto.uri = 'gs://chromeos-prebuilt/target'
    input_proto.chroot.path = self.chroot_path
    input_proto.sysroot.path = self.sysroot_path
    input_proto.uploads_dir = self.uploads_dir
    binhost.PrepareDevInstallBinhostUploads(input_proto, self.response,
                                            self.validate_only_config)
    patch.assert_not_called()

  def testMockCall(self):
    """Test that a mock call does not execute logic, returns mocked value."""
    patch = self.PatchObject(binhost_service,
                             'ReadDevInstallFilesToCreatePackageIndex')

    input_proto = binhost_pb2.PrepareDevInstallBinhostUploadsRequest()
    input_proto.uri = 'gs://chromeos-prebuilt/target'
    input_proto.chroot.path = self.chroot_path
    input_proto.sysroot.path = self.sysroot_path
    input_proto.uploads_dir = self.uploads_dir
    binhost.PrepareDevInstallBinhostUploads(input_proto, self.response,
                                            self.mock_call_config)
    self.assertEqual(len(self.response.upload_targets), 3)
    self.assertEqual(self.response.upload_targets[2].path, 'Packages')
    patch.assert_not_called()

  def testDevInstallerUpload(self):
    """Basic sanity test testing uploads of dev installer prebuilts."""
    # self.RunStage()
    input_proto = binhost_pb2.PrepareDevInstallBinhostUploadsRequest()
    input_proto.uri = 'gs://chromeos-prebuilt/target'
    input_proto.chroot.path = self.chroot_path
    input_proto.sysroot.path = self.sysroot_path
    input_proto.uploads_dir = self.uploads_dir
    # Call method under test
    binhost.PrepareDevInstallBinhostUploads(input_proto, self.response,
                                            self.api_config)
    # Verify results
    expected_upload_targets = ['app-arch/zip-3.0-r3.tbz2',
                               'virtual/chromium-os-printing-1-r4.tbz2',
                               'virtual/python-enum34-1.tbz2',
                               'Packages']
    self.assertCountEqual(
        [ut.path for ut in self.response.upload_targets],
        expected_upload_targets)
    # All of the upload_targets should also be in the uploads_directory
    for target in self.response.upload_targets:
      self.assertExists(os.path.join(input_proto.uploads_dir, target.path))

  def testPrepareBinhostUploadsNonGsUri(self):
    """PrepareBinhostUploads dies when URI does not point to GS."""
    input_proto = binhost_pb2.PrepareDevInstallBinhostUploadsRequest()
    input_proto.chroot.path = self.chroot_path
    input_proto.sysroot.path = self.sysroot_path
    input_proto.uploads_dir = self.uploads_dir
    input_proto.uri = 'https://foo.bar'
    with self.assertRaises(ValueError):
      binhost.PrepareDevInstallBinhostUploads(input_proto, self.response,
                                              self.api_config)
