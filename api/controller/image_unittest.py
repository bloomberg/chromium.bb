# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Image service tests."""

from __future__ import print_function

import os

import mock

from chromite.api import api_config
from chromite.api import controller
from chromite.api.controller import image as image_controller
from chromite.api.gen.chromite.api import image_pb2
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.service import image as image_service


class CreateTest(cros_test_lib.MockTempDirTestCase, api_config.ApiConfigMixin):
  """Create image tests."""

  def setUp(self):
    self.response = image_pb2.CreateImageResult()

  def _GetRequest(self, board=None, types=None, version=None, builder_path=None,
                  disable_rootfs_verification=False):
    """Helper to build a request instance."""
    return image_pb2.CreateImageRequest(
        build_target={'name': board},
        image_types=types,
        disable_rootfs_verification=disable_rootfs_verification,
        version=version,
        builder_path=builder_path,
    )

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(image_service, 'Build')

    request = self._GetRequest(board='board')
    image_controller.Create(request, self.response, self.validate_only_config)
    patch.assert_not_called()

  def testNoBoard(self):
    """Test no board given fails."""
    request = self._GetRequest()

    # No board should cause it to fail.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Create(request, self.response, self.api_config)

  def testNoTypeSpecified(self):
    """Test the image type default."""
    request = self._GetRequest(board='board')

    # Failed result to avoid the success handling logic.
    result = image_service.BuildResult(1, [])
    build_patch = self.PatchObject(image_service, 'Build', return_value=result)

    image_controller.Create(request, self.response, self.api_config)
    build_patch.assert_called_with(images=[constants.IMAGE_TYPE_BASE],
                                   board='board', config=mock.ANY)

  def testSingleTypeSpecified(self):
    """Test it's properly using a specified type."""
    request = self._GetRequest(board='board', types=[common_pb2.DEV])

    # Failed result to avoid the success handling logic.
    result = image_service.BuildResult(1, [])
    build_patch = self.PatchObject(image_service, 'Build', return_value=result)

    image_controller.Create(request, self.response, self.api_config)
    build_patch.assert_called_with(images=[constants.IMAGE_TYPE_DEV],
                                   board='board', config=mock.ANY)

  def testMultipleAndImpliedTypes(self):
    """Test multiple types and implied type handling."""
    # The TEST_VM type should force it to build the test image.
    types = [common_pb2.BASE, common_pb2.TEST_VM]
    expected_images = [constants.IMAGE_TYPE_BASE, constants.IMAGE_TYPE_TEST]

    request = self._GetRequest(board='board', types=types)

    # Failed result to avoid the success handling logic.
    result = image_service.BuildResult(1, [])
    build_patch = self.PatchObject(image_service, 'Build', return_value=result)

    image_controller.Create(request, self.response, self.api_config)
    build_patch.assert_called_with(images=expected_images, board='board',
                                   config=mock.ANY)

  def testFailedPackageHandling(self):
    """Test failed packages are populated correctly."""
    result = image_service.BuildResult(1, ['foo/bar', 'cat/pkg'])
    expected_packages = [('foo', 'bar'), ('cat', 'pkg')]
    self.PatchObject(image_service, 'Build', return_value=result)

    input_proto = self._GetRequest(board='board')

    rc = image_controller.Create(input_proto, self.response, self.api_config)

    self.assertEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE, rc)
    for package in self.response.failed_packages:
      self.assertIn((package.category, package.package_name), expected_packages)

  def testNoPackagesFailureHandling(self):
    """Test failed packages are populated correctly."""
    result = image_service.BuildResult(1, [])
    self.PatchObject(image_service, 'Build', return_value=result)

    input_proto = image_pb2.CreateImageRequest()
    input_proto.build_target.name = 'board'

    rc = image_controller.Create(input_proto, self.response, self.api_config)
    self.assertTrue(rc)
    self.assertNotEqual(controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE,
                        rc)
    self.assertFalse(self.response.failed_packages)


class ImageSignerTestTest(cros_test_lib.MockTempDirTestCase,
                          api_config.ApiConfigMixin):
  """Image signer test tests."""

  def setUp(self):
    self.image_path = os.path.join(self.tempdir, 'image.bin')
    self.result_directory = os.path.join(self.tempdir, 'results')

    osutils.SafeMakedirs(self.result_directory)
    osutils.Touch(self.image_path)

  def testValidateOnly(self):
    """Sanity check that validate-only calls don't execute any logic."""
    patch = self.PatchObject(image_lib, 'SecurityTest', return_value=True)
    input_proto = image_pb2.TestImageRequest()
    input_proto.image.path = self.image_path
    output_proto = image_pb2.TestImageResult()

    image_controller.SignerTest(input_proto, output_proto,
                                self.validate_only_config)

    patch.assert_not_called()

  def testSignerTestNoImage(self):
    """Test function argument validation."""
    input_proto = image_pb2.TestImageRequest()
    output_proto = image_pb2.TestImageResult()

    # Nothing provided.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.SignerTest(input_proto, output_proto, self.api_config)

  def testSignerTestSuccess(self):
    """Test successful call handling."""
    self.PatchObject(image_lib, 'SecurityTest', return_value=True)
    input_proto = image_pb2.TestImageRequest()
    input_proto.image.path = self.image_path
    output_proto = image_pb2.TestImageResult()

    image_controller.SignerTest(input_proto, output_proto, self.api_config)

  def testSignerTestFailure(self):
    """Test function output tests."""
    input_proto = image_pb2.TestImageRequest()
    input_proto.image.path = self.image_path
    output_proto = image_pb2.TestImageResult()

    self.PatchObject(image_lib, 'SecurityTest', return_value=False)
    image_controller.SignerTest(input_proto, output_proto, self.api_config)
    self.assertFalse(output_proto.success)


class ImageTestTest(cros_test_lib.MockTempDirTestCase,
                    api_config.ApiConfigMixin):
  """Image test tests."""

  def setUp(self):
    self.image_path = os.path.join(self.tempdir, 'image.bin')
    self.board = 'board'
    self.result_directory = os.path.join(self.tempdir, 'results')

    osutils.SafeMakedirs(self.result_directory)
    osutils.Touch(self.image_path)

  def testValidateOnly(self):
    """Sanity check that a validate only call does not execute any logic."""
    patch = self.PatchObject(image_service, 'Test')

    input_proto = image_pb2.TestImageRequest()
    input_proto.image.path = self.image_path
    input_proto.build_target.name = self.board
    input_proto.result.directory = self.result_directory
    output_proto = image_pb2.TestImageResult()

    image_controller.Test(input_proto, output_proto, self.validate_only_config)
    patch.assert_not_called()

  def testTestArgumentValidation(self):
    """Test function argument validation tests."""
    self.PatchObject(image_service, 'Test', return_value=True)
    input_proto = image_pb2.TestImageRequest()
    output_proto = image_pb2.TestImageResult()

    # Nothing provided.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Test(input_proto, output_proto, self.api_config)

    # Just one argument.
    input_proto.build_target.name = self.board
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Test(input_proto, output_proto, self.api_config)

    # Two arguments provided.
    input_proto.result.directory = self.result_directory
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Test(input_proto, output_proto, self.api_config)

    # Invalid image path.
    input_proto.image.path = '/invalid/image/path'
    with self.assertRaises(cros_build_lib.DieSystemExit):
      image_controller.Test(input_proto, output_proto, self.api_config)

    # All valid arguments.
    input_proto.image.path = self.image_path
    image_controller.Test(input_proto, output_proto, self.api_config)

  def testTestOutputHandling(self):
    """Test function output tests."""
    input_proto = image_pb2.TestImageRequest()
    input_proto.image.path = self.image_path
    input_proto.build_target.name = self.board
    input_proto.result.directory = self.result_directory
    output_proto = image_pb2.TestImageResult()

    self.PatchObject(image_service, 'Test', return_value=True)
    image_controller.Test(input_proto, output_proto, self.api_config)
    self.assertTrue(output_proto.success)

    self.PatchObject(image_service, 'Test', return_value=False)
    image_controller.Test(input_proto, output_proto, self.api_config)
    self.assertFalse(output_proto.success)
