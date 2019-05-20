# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Portage Binhost operations."""

from __future__ import print_function

import urlparse

from chromite.api.gen.chromite.api import binhost_pb2
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.service import binhost


def PrepareBinhostUploads(input_proto, output_proto):
  """Return a list of files to uplooad to the binhost.

  See BinhostService documentation in api/proto/binhost.proto.

  Args:
    input_proto (PrepareBinhostUploadsRequest): The input proto.
    output_proto (PrepareBinhostUploadsResponse): The output proto.
  """
  target = input_proto.build_target.name
  uri = input_proto.uri

  # For now, we enforce that all input URIs are Google Storage buckets.
  if not gs.PathIsGs(uri):
    raise ValueError('Upload URI %s must be Google Storage.' % uri)

  parsed_uri = urlparse.urlparse(uri)
  upload_uri = gs.GetGsURL(parsed_uri.netloc)
  upload_path = parsed_uri.path.lstrip('/')

  # Read all packages and update the index. The index must be uploaded to the
  # binhost for Portage to use it, so include it in upload_targets.
  uploads_dir = binhost.GetPrebuiltsRoot(target)
  upload_targets = binhost.GetPrebuiltsFiles(uploads_dir)
  index_path = binhost.UpdatePackageIndex(uploads_dir, upload_uri, upload_path,
                                          sudo=True)
  assert index_path.startswith(uploads_dir), (
      'expected index_path to start with uploads_dir')
  upload_targets.append(index_path[len(uploads_dir):])

  output_proto.uploads_dir = uploads_dir
  for upload_target in upload_targets:
    output_proto.upload_targets.add().path = upload_target.strip('/')


def SetBinhost(input_proto, output_proto):
  """Set the URI for a given binhost key and build target.

  See BinhostService documentation in api/proto/binhost.proto.

  Args:
    input_proto (SetBinhostRequest): The input proto.
    output_proto (SetBinhostResponse): The output proto.
  """
  target = input_proto.build_target.name
  key = binhost_pb2.BinhostKey.Name(input_proto.key)
  uri = input_proto.uri
  private = input_proto.private

  # Temporary measure to force the new parallel cq post submit builders to write
  # to a different file than the old ones. Writing to the same file was causing
  # them to fight over the new one's value and the old logic of clearing out
  # the values for files it didn't update. Once we've done a full switch over,
  # we can dump this logic and delete all of the PARALLEL_POSTSUBMIT_BINHOST
  # configs.
  # TODO(crbug.com/965244) remove this.
  if key == 'POSTSUBMIT_BINHOST':
    key = 'PARALLEL_POSTSUBMIT_BINHOST'

  output_proto.output_file = binhost.SetBinhost(target, key, uri,
                                                private=private)

_OVERLAY_TYPE_TO_NAME = {
    binhost_pb2.OVERLAYTYPE_PUBLIC: constants.PUBLIC_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_PRIVATE: constants.PRIVATE_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_BOTH: constants.BOTH_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_NONE: None
}

def RegenBuildCache(input_proto):
  """Regenerate the Build Cache for a build target.

  See BinhostService documentation in api/proto/binhost.proto.

  Args:
    input_proto (RegenBuildCacheRequest): The input proto.
    output_proto (RegenBuildCacheResponse): The output proto.
  """
  overlay_type = input_proto.overlay_type
  sysroot = input_proto.sysroot
  if overlay_type in _OVERLAY_TYPE_TO_NAME:
    binhost.RegenBuildCache(_OVERLAY_TYPE_TO_NAME[overlay_type], sysroot.path)
  else:
    cros_build_lib.Die('Overlay_type must be specified.')
