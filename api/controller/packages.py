# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package related functionality."""

from __future__ import print_function

from chromite.api import validate
from chromite.api.controller import controller_util
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import build_target_util
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.service import packages


_OVERLAY_TYPE_TO_NAME = {
    binhost_pb2.OVERLAYTYPE_PUBLIC: constants.PUBLIC_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_PRIVATE: constants.PRIVATE_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_BOTH: constants.BOTH_OVERLAYS,
}


def Uprev(input_proto, _output_proto):
  """Uprev all cros workon ebuilds that have changes."""
  if not input_proto.overlay_type:
    cros_build_lib.Die('Overlay type is required.')
  elif input_proto.overlay_type not in _OVERLAY_TYPE_TO_NAME:
    cros_build_lib.Die('Overlay type must be one of: %s',
                       ', '.join(_OVERLAY_TYPE_TO_NAME.values()))

  target_names = [t.name for t in input_proto.build_targets]
  build_targets = [build_target_util.BuildTarget(t) for t in target_names]
  overlay_type = _OVERLAY_TYPE_TO_NAME[input_proto.overlay_type]
  chroot = controller_util.ParseChroot(input_proto.chroot)
  output_dir = input_proto.output_dir or None

  try:
    packages.uprev_build_targets(build_targets, overlay_type, chroot,
                                 output_dir)
  except packages.Error as e:
    # Handle module errors nicely, let everything else bubble up.
    cros_build_lib.Die(e.message)

@validate.require('atom')
def GetBestVisible(input_proto, output_proto):
  """Returns the best visible PackageInfo for the indicated atom."""
  cpv = packages.get_best_visible(input_proto.atom)
  package_info = common_pb2.PackageInfo()
  controller_util.CPVToPackageInfo(cpv, package_info)
  output_proto.package_info.CopyFrom(package_info)
