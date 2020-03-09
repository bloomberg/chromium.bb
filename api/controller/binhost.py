# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Portage Binhost operations."""

from __future__ import print_function

import os
import shutil
import sys

from six.moves import urllib

from chromite.api import controller
from chromite.api import faux
from chromite.api import validate
from chromite.api.controller import controller_util
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import sysroot_lib
from chromite.service import binhost


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


_OVERLAY_TYPE_TO_NAME = {
    binhost_pb2.OVERLAYTYPE_PUBLIC: constants.PUBLIC_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_PRIVATE: constants.PRIVATE_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_BOTH: constants.BOTH_OVERLAYS,
    binhost_pb2.OVERLAYTYPE_NONE: None
}


def _GetBinhostsResponse(_input_proto, output_proto, _config):
  """Add fake binhosts to a successful response."""
  new_binhost = output_proto.binhosts.add()
  new_binhost.uri = ('gs://cr-prebuilt/board/amd64-generic/'
                     'paladin-R66-17.0.0-rc2/packages/')
  new_binhost.package_index = 'Packages'


@faux.success(_GetBinhostsResponse)
@faux.empty_error
@validate.require('build_target.name')
@validate.validation_complete
def GetBinhosts(input_proto, output_proto, _config):
  """Get a list of binhosts."""
  build_target = controller_util.ParseBuildTarget(input_proto.build_target)

  binhosts = binhost.GetBinhosts(build_target)

  for current in binhosts:
    new_binhost = output_proto.binhosts.add()
    new_binhost.uri = current
    new_binhost.package_index = 'Packages'


def _GetPrivatePrebuiltAclArgsResponse(_input_proto, output_proto, _config):
  """Add fake acls to a successful response."""
  new_arg = output_proto.args.add()
  new_arg.arg = '-g'
  new_arg.value = 'group1:READ'


@faux.success(_GetPrivatePrebuiltAclArgsResponse)
@faux.empty_error
@validate.require('build_target.name')
@validate.validation_complete
def GetPrivatePrebuiltAclArgs(input_proto, output_proto, _config):
  """Get the ACL args from the files in the private overlays."""
  build_target = controller_util.ParseBuildTarget(input_proto.build_target)

  try:
    args = binhost.GetPrebuiltAclArgs(build_target)
  except binhost.Error as e:
    cros_build_lib.Die(e)

  for arg, value in args:
    new_arg = output_proto.args.add()
    new_arg.arg = arg
    new_arg.value = value


def _PrepareBinhostUploadsResponse(_input_proto, output_proto, _config):
  """Add fake binhost upload targets to a successful response."""
  output_proto.uploads_dir = '/upload/directory'
  output_proto.upload_targets.add().path = 'upload_target'


@faux.success(_PrepareBinhostUploadsResponse)
@faux.empty_error
@validate.require('uri')
def PrepareBinhostUploads(input_proto, output_proto, config):
  """Return a list of files to upload to the binhost.

  See BinhostService documentation in api/proto/binhost.proto.

  Args:
    input_proto (PrepareBinhostUploadsRequest): The input proto.
    output_proto (PrepareBinhostUploadsResponse): The output proto.
    config (api_config.ApiConfig): The API call config.
  """
  if input_proto.sysroot.build_target.name:
    build_target_msg = input_proto.sysroot.build_target
  else:
    build_target_msg = input_proto.build_target
  sysroot_path = input_proto.sysroot.path

  if not sysroot_path and not build_target_msg.name:
    cros_build_lib.Die('Sysroot.path is required.')

  build_target = controller_util.ParseBuildTarget(build_target_msg)
  chroot = controller_util.ParseChroot(input_proto.chroot)

  if not sysroot_path:
    sysroot_path = build_target.root
  sysroot = sysroot_lib.Sysroot(sysroot_path)

  uri = input_proto.uri
  # For now, we enforce that all input URIs are Google Storage buckets.
  if not gs.PathIsGs(uri):
    raise ValueError('Upload URI %s must be Google Storage.' % uri)

  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  parsed_uri = urllib.parse.urlparse(uri)
  upload_uri = gs.GetGsURL(parsed_uri.netloc, for_gsutil=True).rstrip('/')
  upload_path = parsed_uri.path.lstrip('/')

  # Read all packages and update the index. The index must be uploaded to the
  # binhost for Portage to use it, so include it in upload_targets.
  uploads_dir = binhost.GetPrebuiltsRoot(chroot, sysroot, build_target)
  index_path = binhost.UpdatePackageIndex(uploads_dir, upload_uri, upload_path,
                                          sudo=True)
  upload_targets = binhost.GetPrebuiltsFiles(uploads_dir)
  assert index_path.startswith(uploads_dir), (
      'expected index_path to start with uploads_dir')
  upload_targets.append(index_path[len(uploads_dir):])

  output_proto.uploads_dir = uploads_dir
  for upload_target in upload_targets:
    output_proto.upload_targets.add().path = upload_target.strip('/')


def _PrepareDevInstallBinhostUploadsResponse(_input_proto, output_proto,
                                             _config):
  """Add fake binhost files to a successful response."""
  output_proto.upload_targets.add().path = 'app-arch/zip-3.0-r3.tbz2'
  output_proto.upload_targets.add().path = 'virtual/python-enum34-1.tbz2'
  output_proto.upload_targets.add().path = 'Packages'


@faux.success(_PrepareDevInstallBinhostUploadsResponse)
@faux.empty_error
@validate.require('uri', 'sysroot.path')
@validate.exists('uploads_dir')
def PrepareDevInstallBinhostUploads(input_proto, output_proto, config):
  """Return a list of files to upload to the binhost"

  The files will also be copied to the uploads_dir.
  See BinhostService documentation in api/proto/binhost.proto.

  Args:
    input_proto (PrepareDevInstallBinhostUploadsRequest): The input proto.
    output_proto (PrepareDevInstallBinhostUploadsResponse): The output proto.
    config (api_config.ApiConfig): The API call config.
  """
  sysroot_path = input_proto.sysroot.path

  chroot = controller_util.ParseChroot(input_proto.chroot)
  sysroot = sysroot_lib.Sysroot(sysroot_path)

  uri = input_proto.uri
  # For now, we enforce that all input URIs are Google Storage buckets.
  if not gs.PathIsGs(uri):
    raise ValueError('Upload URI %s must be Google Storage.' % uri)

  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  parsed_uri = urllib.parse.urlparse(uri)
  upload_uri = gs.GetGsURL(parsed_uri.netloc, for_gsutil=True).rstrip('/')
  upload_path = parsed_uri.path.lstrip('/')

  # Calculate the filename for the to-be-created Packages file, which will
  # contain only devinstall packages.
  devinstall_package_index_path = os.path.join(input_proto.uploads_dir,
                                               'Packages')
  upload_targets_list = binhost.ReadDevInstallFilesToCreatePackageIndex(
      chroot, sysroot, devinstall_package_index_path, upload_uri, upload_path)

  package_dir = chroot.full_path(sysroot.path, 'packages')
  for upload_target in upload_targets_list:
    # Copy each package to target/category/package
    upload_target = upload_target.strip('/')
    category = upload_target.split(os.sep)[0]
    target_dir = os.path.join(input_proto.uploads_dir, category)
    if not os.path.exists(target_dir):
      os.makedirs(target_dir)
    full_src_pkg_path = os.path.join(package_dir, upload_target)
    full_target_src_path = os.path.join(input_proto.uploads_dir, upload_target)
    shutil.copyfile(full_src_pkg_path, full_target_src_path)
    output_proto.upload_targets.add().path = upload_target
  output_proto.upload_targets.add().path = 'Packages'


def _SetBinhostResponse(_input_proto, output_proto, _config):
  """Add fake binhost file to a successful response."""
  output_proto.output_file = '/path/to/BINHOST.conf'


@faux.success(_SetBinhostResponse)
@faux.empty_error
@validate.require('build_target.name', 'key', 'uri')
@validate.validation_complete
def SetBinhost(input_proto, output_proto, _config):
  """Set the URI for a given binhost key and build target.

  See BinhostService documentation in api/proto/binhost.proto.

  Args:
    input_proto (SetBinhostRequest): The input proto.
    output_proto (SetBinhostResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  target = input_proto.build_target.name
  key = binhost_pb2.BinhostKey.Name(input_proto.key)
  uri = input_proto.uri
  private = input_proto.private

  output_proto.output_file = binhost.SetBinhost(target, key, uri,
                                                private=private)


def _RegenBuildCacheResponse(_input_proto, output_proto, _config):
  """Add fake binhosts cache path to a successful response."""
  output_proto.modified_overlays.add().path = '/path/to/BuildCache'


@faux.success(_RegenBuildCacheResponse)
@faux.empty_error
@validate.require('overlay_type')
@validate.is_in('overlay_type', _OVERLAY_TYPE_TO_NAME)
@validate.validation_complete
def RegenBuildCache(input_proto, output_proto, _config):
  """Regenerate the Build Cache for a build target.

  See BinhostService documentation in api/proto/binhost.proto.

  Args:
    input_proto (RegenBuildCacheRequest): The input proto.
    output_proto (RegenBuildCacheResponse): The output proto.
    _config (api_config.ApiConfig): The API call config.
  """
  chroot = controller_util.ParseChroot(input_proto.chroot)
  overlay_type = input_proto.overlay_type
  overlays = binhost.RegenBuildCache(chroot,
                                     _OVERLAY_TYPE_TO_NAME[overlay_type])

  for overlay in overlays:
    output_proto.modified_overlays.add().path = overlay
