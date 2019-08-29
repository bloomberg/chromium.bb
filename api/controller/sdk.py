# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SDK chroot operations."""

from __future__ import print_function

import os

from chromite.api import controller
from chromite.api import faux
from chromite.api import validate
from chromite.lib import cros_build_lib
from chromite.service import sdk


def _ChrootVersionResponse(_input_proto, output_proto, _config):
  """Add a fake chroot version to a successful response."""
  output_proto.version.version = 168


@faux.success(_ChrootVersionResponse)
@faux.empty_error
def Create(input_proto, output_proto, config):
  """Chroot creation, includes support for replacing an existing chroot.

  Args:
    input_proto (CreateRequest): The input proto.
    output_proto (CreateResponse): The output proto.
    config (api_config.ApiConfig): The API call config.
  """
  replace = not input_proto.flags.no_replace
  bootstrap = input_proto.flags.bootstrap
  use_image = not input_proto.flags.no_use_image

  chroot_path = input_proto.chroot.path
  cache_dir = input_proto.chroot.cache_dir

  if chroot_path and not os.path.isabs(chroot_path):
    cros_build_lib.Die('The chroot path must be absolute.')

  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  paths = sdk.ChrootPaths(cache_dir=cache_dir, chroot_path=chroot_path)
  args = sdk.CreateArguments(replace=replace, bootstrap=bootstrap,
                             use_image=use_image, paths=paths)

  version = sdk.Create(args)

  if version:
    output_proto.version.version = version
  else:
    # This should be very rare, if ever used, but worth noting.
    cros_build_lib.Die('No chroot version could be found. There was likely an'
                       'error creating the chroot that was not detected.')


@faux.success(_ChrootVersionResponse)
@faux.empty_error
@validate.validation_complete
def Update(input_proto, output_proto, _config):
  """Update the chroot.

  Args:
    input_proto (UpdateRequest): The input proto.
    output_proto (UpdateResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  build_source = input_proto.flags.build_source
  targets = [target.name for target in input_proto.toolchain_targets]

  args = sdk.UpdateArguments(build_source=build_source,
                             toolchain_targets=targets)
  version = sdk.Update(args)

  if version:
    output_proto.version.version = version
  else:
    # This should be very rare, if ever used, but worth noting.
    cros_build_lib.Die('No chroot version could be found. There was likely an'
                       'error creating the chroot that was not detected.')
