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

from chromite.api.controller import controller_util
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import cros_logging as logging
from chromite.lib import osutils

from google.protobuf import message as protobuf_message


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
    return controller_util.ParseChroot(chroot_message)


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
    # For resetting the state.
    self._transferred = False
    self._original_message = common_pb2.Path()
    self._original_message.CopyFrom(self.field)

  def transfer(self, direction=None):
    """Copy the file or directory to its destination.

    Args:
      direction (int): The direction files are being copied (into or out of
        the chroot). Specifying the direction allows avoiding performing
        unnecessary copies.
    """
    if self._transferred:
      return

    if direction is None:
      direction = self.ALL
    assert direction in [self.INSIDE, self.OUTSIDE, self.ALL]

    if self.field.location == direction:
      # Already in the correct location, nothing to do.
      return

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

    self.field.path = return_path
    self.field.location = direction
    self._transferred = True

  def cleanup(self):
    if self.tempdir:
      self.tempdir.Cleanup()
      self.tempdir = None

    self.field.CopyFrom(self._original_message)


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

  handlers = _extract_handlers(message, destination, delete, prefix)

  for handler in handlers:
    handler.transfer(direction)

  try:
    yield handlers
  finally:
    for handler in handlers:
      handler.cleanup()


def _extract_handlers(message, destination, delete, prefix):
  """Recursive helper for handle_paths to extract Path messages."""
  handlers = []
  for descriptor in message.DESCRIPTOR.fields:
    field = getattr(message, descriptor.name)
    if isinstance(field, common_pb2.Path):
      if not field.path or not field.location:
        logging.debug('Skipping %s; incomplete.', descriptor.name)
        continue

      handler = PathHandler(field, destination, delete=delete, prefix=prefix)
      handlers.append(handler)
    elif isinstance(field, protobuf_message.Message):
      handlers.extend(_extract_handlers(field, destination, delete, prefix))

  return handlers
