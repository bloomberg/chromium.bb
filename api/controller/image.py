# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Image API Service.

The image related API endpoints should generally be found here.
"""

from __future__ import print_function

import os

from chromite.api import controller
from chromite.api import faux
from chromite.api import validate
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import cros_build_lib
from chromite.lib import constants
from chromite.lib import image_lib
from chromite.service import image

# The image.proto ImageType enum ids.
_BASE_ID = common_pb2.BASE
_DEV_ID = common_pb2.DEV
_TEST_ID = common_pb2.TEST
_BASE_VM_ID = common_pb2.BASE_VM
_TEST_VM_ID = common_pb2.TEST_VM
_RECOVERY_ID = common_pb2.RECOVERY
_FACTORY_ID = common_pb2.FACTORY
_FIRMWARE_ID = common_pb2.FIRMWARE

# Dict to allow easily translating names to enum ids and vice versa.
_IMAGE_MAPPING = {
    _BASE_ID: constants.IMAGE_TYPE_BASE,
    constants.IMAGE_TYPE_BASE: _BASE_ID,
    _DEV_ID: constants.IMAGE_TYPE_DEV,
    constants.IMAGE_TYPE_DEV: _DEV_ID,
    _TEST_ID: constants.IMAGE_TYPE_TEST,
    constants.IMAGE_TYPE_TEST: _TEST_ID,
    _RECOVERY_ID: constants.IMAGE_TYPE_RECOVERY,
    constants.IMAGE_TYPE_RECOVERY: _RECOVERY_ID,
    _FACTORY_ID: constants.IMAGE_TYPE_FACTORY,
    constants.IMAGE_TYPE_FACTORY: _FACTORY_ID,
    _FIRMWARE_ID: constants.IMAGE_TYPE_FIRMWARE,
    constants.IMAGE_TYPE_FIRMWARE: _FIRMWARE_ID,
}

_VM_IMAGE_MAPPING = {
    _BASE_VM_ID: _IMAGE_MAPPING[_BASE_ID],
    _TEST_VM_ID: _IMAGE_MAPPING[_TEST_ID],
}


@faux.all_empty
@validate.require('build_target.name')
@validate.validation_complete
def Create(input_proto, output_proto, _config):
  """Build an image.

  Args:
    input_proto (image_pb2.CreateImageRequest): The input message.
    output_proto (image_pb2.CreateImageResult): The output message.
    _config (api_config.ApiConfig): The API call config.
  """
  board = input_proto.build_target.name

  # Build the base image if no images provided.
  to_build = input_proto.image_types or [_BASE_ID]

  image_types, vm_types = _ParseImagesToCreate(to_build)
  build_config = _ParseCreateBuildConfig(input_proto)

  # Sorted isn't really necessary here, but it's much easier to test.
  result = image.Build(board=board, images=sorted(list(image_types)),
                       config=build_config)

  output_proto.success = result.success
  if result.success:
    # Success -- we need to list out the images we built in the output.
    _PopulateBuiltImages(board, image_types, output_proto)
  else:
    # Failure, include all of the failed packages in the output when available.
    if not result.failed_packages:
      return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY

    for package in result.failed_packages:
      current = output_proto.failed_packages.add()
      current.category = package.category
      current.package_name = package.package
      if package.version:
        current.version = package.version

    return controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE

  if not vm_types:
    # No VMs to build, we can exit now.
    return controller.RETURN_CODE_SUCCESS

  # There can be only one.
  vm_type = vm_types.pop()
  is_test = vm_type == _TEST_VM_ID
  try:
    vm_path = image.CreateVm(board, is_test=is_test)
  except image.ImageToVmError as e:
    cros_build_lib.Die(e.message)

  new_image = output_proto.images.add()
  new_image.path = vm_path
  new_image.type = vm_type
  new_image.build_target.name = board


def _ParseImagesToCreate(to_build):
  """Helper function to parse the image types to build.

  This function exists just to clean up the Create function.

  Args:
    to_build (list[int]): The image type list.

  Returns:
    (set, set): The image and vm types, respectively, that need to be built.
  """
  image_types = set()
  vm_types = set()
  for current in to_build:
    if current in _IMAGE_MAPPING:
      image_types.add(_IMAGE_MAPPING[current])
    elif current in _VM_IMAGE_MAPPING:
      vm_types.add(current)
      # Make sure we build the image required to build the VM.
      image_types.add(_VM_IMAGE_MAPPING[current])
    else:
      # Not expected, but at least it will be obvious if this comes up.
      cros_build_lib.Die(
          "The service's known image types do not match those in image.proto. "
          'Unknown Enum ID: %s' % current)

  if len(vm_types) > 1:
    cros_build_lib.Die('Cannot create more than one VM.')

  return image_types, vm_types


def _ParseCreateBuildConfig(input_proto):
  """Helper to parse the image build config for Create."""
  enable_rootfs_verification = not input_proto.disable_rootfs_verification
  version = input_proto.version or None
  disk_layout = input_proto.disk_layout or None
  builder_path = input_proto.builder_path or None
  return image.BuildConfig(
      enable_rootfs_verification=enable_rootfs_verification, replace=True,
      version=version, disk_layout=disk_layout, builder_path=builder_path,
  )


def _PopulateBuiltImages(board, image_types, output_proto):
  """Helper to list out built images for Create."""
  # Build out the ImageType->ImagePath mapping in the output.
  # We're using the default path, so just fetch that, but read the symlink so
  # the path we're returning is somewhat more permanent.
  latest_link = image_lib.GetLatestImageLink(board)
  base_path = os.path.realpath(latest_link)

  for current in image_types:
    type_id = _IMAGE_MAPPING[current]
    path = os.path.join(base_path, constants.IMAGE_TYPE_TO_NAME[current])

    new_image = output_proto.images.add()
    new_image.path = path
    new_image.type = type_id
    new_image.build_target.name = board


@faux.all_empty
@validate.exists('image.path')
@validate.validation_complete
def SignerTest(input_proto, output_proto, _config):
  """Run image tests.

  Args:
    input_proto (image_pb2.ImageTestRequest): The input message.
    output_proto (image_pb2.ImageTestResult): The output message.
    _config (api_config.ApiConfig): The API call config.
  """
  image_path = input_proto.image.path

  result = image_lib.SecurityTest(image=image_path)
  output_proto.success = result
  if result:
    return controller.RETURN_CODE_SUCCESS
  else:
    return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY


@faux.all_empty
@validate.require('build_target.name', 'result.directory')
@validate.exists('image.path')
def Test(input_proto, output_proto, config):
  """Run image tests.

  Args:
    input_proto (image_pb2.ImageTestRequest): The input message.
    output_proto (image_pb2.ImageTestResult): The output message.
    config (api_config.ApiConfig): The API call config.
  """
  image_path = input_proto.image.path
  board = input_proto.build_target.name
  result_directory = input_proto.result.directory

  if not os.path.isfile(image_path) or not image_path.endswith('.bin'):
    cros_build_lib.Die(
        'The image.path must be an existing image file with a .bin extension.')

  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  success = image.Test(board, result_directory, image_dir=image_path)
  output_proto.success = success

  if success:
    return controller.RETURN_CODE_SUCCESS
  else:
    return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY
