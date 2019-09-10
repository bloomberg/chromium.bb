# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""packages controller unit tests."""

from __future__ import print_function

import mock

from chromite.api.api_config import ApiConfigMixin
from chromite.api.controller import packages as packages_controller
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.api.gen.chromite.api import packages_pb2
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import portage_util
from chromite.service import packages as packages_service


class UprevTest(cros_test_lib.MockTestCase, ApiConfigMixin):
  """Uprev tests."""

  _PUBLIC = binhost_pb2.OVERLAYTYPE_PUBLIC
  _PRIVATE = binhost_pb2.OVERLAYTYPE_PRIVATE
  _BOTH = binhost_pb2.OVERLAYTYPE_BOTH
  _NONE = binhost_pb2.OVERLAYTYPE_NONE

  def setUp(self):
    self.uprev_patch = self.PatchObject(packages_service, 'uprev_build_targets')
    self.response = packages_pb2.UprevPackagesResponse()

  def _GetRequest(self, targets=None, overlay_type=None, output_dir=None):
    return packages_pb2.UprevPackagesRequest(
        build_targets=[{'name': name} for name in targets or []],
        overlay_type=overlay_type,
        output_dir=output_dir,
    )

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(packages_service, 'uprev_build_targets')

    targets = ['foo', 'bar']
    request = self._GetRequest(targets=targets, overlay_type=self._BOTH)
    packages_controller.Uprev(request, self.response, self.validate_only_config)
    patch.assert_not_called()

  def testNoOverlayTypeFails(self):
    """No overlay type provided should fail."""
    request = self._GetRequest(targets=['foo'])

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.Uprev(request, self.response, self.api_config)

  def testOverlayTypeNoneFails(self):
    """Overlay type none means nothing here and should fail."""
    request = self._GetRequest(targets=['foo'], overlay_type=self._NONE)

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.Uprev(request, self.response, self.api_config)

  def testSuccess(self):
    """Test overall successful argument handling."""
    targets = ['foo', 'bar']
    output_dir = '/tmp/uprev_output_dir'
    changed = ['/ebuild-1.0-r1.ebuild', '/ebuild-1.0-r2.ebuild']
    expected_type = constants.BOTH_OVERLAYS
    request = self._GetRequest(targets=targets, overlay_type=self._BOTH,
                               output_dir=output_dir)
    uprev_patch = self.PatchObject(packages_service, 'uprev_build_targets',
                                   return_value=changed)

    packages_controller.Uprev(request, self.response, self.api_config)

    # Make sure the type is right, verify build targets after.
    uprev_patch.assert_called_once_with(mock.ANY, expected_type, mock.ANY,
                                        output_dir)
    # First argument (build targets) of the first (only) call.
    call_targets = uprev_patch.call_args[0][0]
    self.assertItemsEqual(targets, [t.name for t in call_targets])

    for ebuild in self.response.modified_ebuilds:
      self.assertIn(ebuild.path, changed)
      changed.remove(ebuild.path)
    self.assertFalse(changed)


class UprevVersionedPackageTest(cros_test_lib.MockTestCase, ApiConfigMixin):
  """UprevVersionedPackage tests."""

  def setUp(self):
    self.response = packages_pb2.UprevVersionedPackageResponse()

  def _addVersion(self, request, version):
    """Helper method to add a full version message to the request."""
    ref = request.versions.add()
    ref.repository = '/some/path'
    ref.ref = 'refs/tags/%s' % version
    ref.revision = 'abc123'

  def testValidateOnly(self):
    """Sanity check validate only calls are working properly."""
    service = self.PatchObject(packages_service, 'uprev_versioned_package')

    request = packages_pb2.UprevVersionedPackageRequest()
    self._addVersion(request, '1.2.3.4')
    request.package_info.category = 'chromeos-base'
    request.package_info.package_name = 'chromeos-chrome'

    packages_controller.UprevVersionedPackage(request, self.response,
                                              self.validate_only_config)

    service.assert_not_called()

  def testNoVersions(self):
    """Test no versions provided."""
    request = packages_pb2.UprevVersionedPackageRequest()
    request.package_info.category = 'chromeos-base'
    request.package_info.package_name = 'chromeos-chrome'

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.UprevVersionedPackage(request, self.response,
                                                self.api_config)

  def testNoPackageName(self):
    """Test no package name provided."""
    request = packages_pb2.UprevVersionedPackageRequest()
    self._addVersion(request, '1.2.3.4')
    request.package_info.category = 'chromeos-base'

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.UprevVersionedPackage(request, self.response,
                                                self.api_config)

  def testNoCategory(self):
    """Test no package category provided."""
    request = packages_pb2.UprevVersionedPackageRequest()
    self._addVersion(request, '1.2.3.4')
    request.package_info.package_name = 'chromeos-chrome'

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.UprevVersionedPackage(request, self.response,
                                                self.api_config)

  def testOutputHandling(self):
    """Test the modified files are getting correctly added to the output."""
    version = '1.2.3.4'
    result = packages_service.UprevVersionedPackageResult().add_result(
        version, ['/file/one', '/file/two'])

    self.PatchObject(
        packages_service, 'uprev_versioned_package', return_value=result)

    request = packages_pb2.UprevVersionedPackageRequest()
    self._addVersion(request, version)
    request.package_info.category = 'chromeos-base'
    request.package_info.package_name = 'chromeos-chrome'

    packages_controller.UprevVersionedPackage(request, self.response,
                                              self.api_config)

    for idx, uprev_response in enumerate(self.response.responses):
      self.assertEqual(result.modified[idx].new_version, uprev_response.version)
      self.assertItemsEqual(
          result.modified[idx].files,
          [ebuild.path for ebuild in uprev_response.modified_ebuilds])


class GetBestVisibleTest(cros_test_lib.MockTestCase, ApiConfigMixin):
  """GetBestVisible tests."""

  def setUp(self):
    self.response = packages_pb2.GetBestVisibleResponse()

  def _GetRequest(self, atom=None):
    return packages_pb2.GetBestVisibleRequest(
        atom=atom,
    )

  def _MakeCpv(self, category, package, version):
    unused = {
        'cp': None,
        'cpv': None,
        'cpf': None,
        'pv': None,
        'version_no_rev': None,
        'rev': None,
    }
    return portage_util.CPV(
        category=category,
        package=package,
        version=version,
        **unused
    )

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(packages_service, 'get_best_visible')

    request = self._GetRequest(atom='chromeos-chrome')
    packages_controller.GetBestVisible(request, self.response,
                                       self.validate_only_config)
    patch.assert_not_called()

  def testNoAtomFails(self):
    """No atom provided should fail."""
    request = self._GetRequest()
    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.GetBestVisible(request, self.response,
                                         self.api_config)

  def testSuccess(self):
    """Test overall success, argument handling, result forwarding."""
    cpv = self._MakeCpv('category', 'package', 'version')
    self.PatchObject(packages_service, 'get_best_visible', return_value=cpv)

    request = self._GetRequest(atom='chromeos-chrome')

    packages_controller.GetBestVisible(request, self.response, self.api_config)

    package_info = self.response.package_info
    self.assertEqual(package_info.category, cpv.category)
    self.assertEqual(package_info.package_name, cpv.package)
    self.assertEqual(package_info.version, cpv.version)


class HasChromePrebuiltTest(cros_test_lib.MockTestCase, ApiConfigMixin):
  """HasChromePrebuilt tests."""

  def setUp(self):
    self.response = packages_pb2.HasChromePrebuiltResponse()

  def _GetRequest(self, board=None):
    """Helper to build out a request."""
    request = packages_pb2.HasChromePrebuiltRequest()

    if board:
      request.build_target.name = board

    return request

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(packages_service, 'has_prebuilt')

    request = self._GetRequest(board='betty')
    packages_controller.HasChromePrebuilt(request, self.response,
                                          self.validate_only_config)
    patch.assert_not_called()

  def testNoBuildTargetFails(self):
    """No build target argument should fail."""
    request = self._GetRequest()

    with self.assertRaises(cros_build_lib.DieSystemExit):
      packages_controller.HasChromePrebuilt(request, self.response,
                                            self.api_config)
