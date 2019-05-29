# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Field handler classes.

The field handlers are meant to parse information from or do some other generic
action for a specific field type for the build_api script.
"""

from __future__ import print_function

import contextlib
import os
import shutil

from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import chroot_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


class ChrootHandler(object):
  """Translate a Chroot message to chroot enter arguments and env."""

  def __init__(self, clear_field):
    self.clear_field = clear_field

  def handle(self, message):
    """Parse a message for a chroot field."""
    # Find the Chroot field. Search for the field by type to prevent it being
    # tied to a naming convention.
    for descriptor in message.DESCRIPTOR.fields:
      field = getattr(message, descriptor.name)
      if isinstance(field, common_pb2.Chroot):
        chroot = field
        if self.clear_field:
          message.ClearField(descriptor.name)
        return self.parse_chroot(chroot)

    return None

  def parse_chroot(self, chroot_message):
    """Parse a Chroot message instance."""
    path = chroot_message.path or constants.DEFAULT_CHROOT_PATH
    return chroot_lib.Chroot(path=path, cache_dir=chroot_message.cache_dir,
                             env=self._parse_env(chroot_message))

  def _parse_env(self, chroot_message):
    """Get chroot environment variables that need to be set.

    Returns:
      dict - The variable: value pairs.
    """
    use_flags = [u.flag for u in chroot_message.env.use_flags]
    features = [f.feature for f in chroot_message.env.features]

    env = {}
    if use_flags:
      env['USE'] = ' '.join(use_flags)

    # TODO(saklein) Remove the default when fully integrated in recipes.
    env['FEATURES'] = 'separatedebug'
    if features:
      env['FEATURES'] = ' '.join(features)

    return env


def handle_chroot(message, clear_field=True):
  """Find and parse the chroot field, returning the Chroot instance.

  Returns:
    chroot_lib.Chroot
  """
  handler = ChrootHandler(clear_field)
  chroot = handler.handle(message)
  if chroot:
    return chroot

  logging.warning('No chroot message found, falling back to defaults.')
  return handler.parse_chroot(common_pb2.Chroot())


class PathHandler(object):
  """Handles copying a file or directory into or out of the chroot."""

  INSIDE = common_pb2.Path.INSIDE
  OUTSIDE = common_pb2.Path.OUTSIDE
  ALL = -1

  def __init__(self, field, destination, delete, prefix=None):
    """Path handler initialization.

    Args:
      field (common_pb2.Path): The Path message.
      destination (str): The destination base path.
      delete (bool): Whether the copied file(s) should be deleted on cleanup.
      prefix (str|None): A path prefix to remove from the destination path
        when building the new Path message to pass back. This is largely meant
        to support removing the chroot directory for files moved into the chroot
        for endpoints that execute inside.
    """
    assert isinstance(field, common_pb2.Path)
    assert field.path
    assert field.location

    self.field = field
    self.destination = destination
    self.prefix = prefix or ''
    self.delete = delete
    self.tempdir = None

  def transfer(self, direction=None):
    """Copy the file or directory to its destination.

    Args:
      direction (int): The direction files are being copied (into or out of
        the chroot). Specifying the direction allows avoiding performing
        unnecessary copies.
    """
    if direction is None:
      direction = self.ALL
    assert direction in [self.INSIDE, self.OUTSIDE, self.ALL]

    if self.field.location == direction:
      return None

    if self.delete:
      self.tempdir = osutils.TempDir(base_dir=self.destination)
      destination = self.tempdir.tempdir
    else:
      destination = self.destination

    if os.path.isfile(self.field.path):
      # Use the old file name, just copy it into dest.
      dest_path = os.path.join(destination, os.path.basename(self.field.path))
      copy_fn = shutil.copy
    else:
      dest_path = destination
      copy_fn = osutils.CopyDirContents

    logging.debug('Copying %s to %s', self.field.path, dest_path)
    copy_fn(self.field.path, dest_path)

    # Clean up the destination path for returning, if applicable.
    return_path = dest_path
    if return_path.startswith(self.prefix):
      return_path = return_path[len(self.prefix):]

    path = common_pb2.Path()
    path.path = return_path
    path.location = direction

    return path

  def cleanup(self):
    if self.tempdir:
      self.tempdir.Cleanup()
      self.tempdir = None


@contextlib.contextmanager
def handle_paths(message, destination, delete=True, direction=None,
                 prefix=None):
  """Context manager function to transfer and cleanup all Path messages.

  Args:
    message (Message): A message whose Path messages should be transferred.
    destination (str): A base destination path.
    delete (bool): Whether the file(s) should be deleted.
    direction (int): One of the PathHandler constants (INSIDE, OUTSIDE, ALL).
      This allows avoiding unnecessarily copying files already in the right
      place (e.g. copying a file into the chroot that's already in the chroot).
    prefix (str|None): A prefix path to remove from the final destination path
      in the Path message (i.e. remove the chroot path).

  Returns:
    list[PathHandler]: The path handlers.
  """
  assert destination
  direction = direction or PathHandler.ALL

  # field-name, handler pairs.
  handlers = []
  for descriptor in message.DESCRIPTOR.fields:
    field = getattr(message, descriptor.name)
    if isinstance(field, common_pb2.Path):
      if not field.path or not field.location:
        logging.debug('Skipping %s; incomplete.', descriptor.name)
        continue

      handler = PathHandler(field, destination, delete=delete, prefix=prefix)
      handlers.append((descriptor.name, handler))

  for field_name, handler in handlers:
    new_field = handler.transfer(direction)
    if not new_field:
      # When no copy is needed.
      continue

    old_field = getattr(message, field_name)
    old_field.path = new_field.path
    old_field.location = new_field.location

  try:
    yield handlers
  finally:
    for field_name, handler in handlers:
      handler.cleanup()
