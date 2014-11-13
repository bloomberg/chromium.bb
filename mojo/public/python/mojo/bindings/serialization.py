# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility classes for serialization"""

import struct


# Format of a header for a struct or an array.
HEADER_STRUCT = struct.Struct("<II")


class SerializationException(Exception):
  """Error when strying to serialize a struct."""
  pass


class DeserializationException(Exception):
  """Error when strying to deserialize a struct."""
  pass


class Serialization(object):
  """
  Helper class to serialize/deserialize a struct.
  """
  def __init__(self, groups):
    self.version = _GetVersion(groups)
    self._groups = groups
    main_struct = _GetStruct(groups)
    self.size = HEADER_STRUCT.size + main_struct.size
    self._struct_per_version = {
        self.version: main_struct,
    }
    self._groups_per_version = {
        self.version: groups,
    }

  def _GetMainStruct(self):
    return self._GetStruct(self.version)

  def _GetGroups(self, version):
    # If asking for a version greater than the last known.
    version = min(version, self.version)
    if version not in self._groups_per_version:
      self._groups_per_version[version] = _FilterGroups(self._groups, version)
    return self._groups_per_version[version]

  def _GetStruct(self, version):
    # If asking for a version greater than the last known.
    version = min(version, self.version)
    if version not in self._struct_per_version:
      self._struct_per_version[version] = _GetStruct(self._GetGroups(version))
    return self._struct_per_version[version]

  def Serialize(self, obj, handle_offset):
    """
    Serialize the given obj. handle_offset is the the first value to use when
    encoding handles.
    """
    handles = []
    data = bytearray(self.size)
    HEADER_STRUCT.pack_into(data, 0, self.size, self.version)
    position = HEADER_STRUCT.size
    to_pack = []
    for group in self._groups:
      position = position + NeededPaddingForAlignment(position,
                                                      group.GetByteSize())
      (entry, new_handles) = group.Serialize(
          obj,
          len(data) - position,
          data,
          handle_offset + len(handles))
      to_pack.append(entry)
      handles.extend(new_handles)
      position = position + group.GetByteSize()
    self._GetMainStruct().pack_into(data, HEADER_STRUCT.size, *to_pack)
    return (data, handles)

  def Deserialize(self, fields, data, handles):
    if not isinstance(data, buffer):
      data = buffer(data)
    (_, version) = HEADER_STRUCT.unpack_from(data)
    version_struct = self._GetStruct(version)
    entitities = version_struct.unpack_from(data, HEADER_STRUCT.size)
    filtered_groups = self._GetGroups(version)
    position = HEADER_STRUCT.size
    for (group, value) in zip(filtered_groups, entitities):
      position = position + NeededPaddingForAlignment(position,
                                                      group.GetByteSize())
      fields.update(group.Deserialize(value, buffer(data, position), handles))
      position += group.GetByteSize()


def NeededPaddingForAlignment(value, alignment=8):
  """Returns the padding necessary to align value with the given alignment."""
  if value % alignment:
    return alignment - (value % alignment)
  return 0


def _GetVersion(groups):
  return sum([len(x.descriptors) for x in groups])


def _FilterGroups(groups, version):
  return [group for group in groups if group.GetVersion() < version]


def _GetStruct(groups):
  index = 0
  codes = [ '<' ]
  for group in groups:
    code = group.GetTypeCode()
    size = group.GetByteSize()
    needed_padding = NeededPaddingForAlignment(index, size)
    if needed_padding:
      codes.append('x' * needed_padding)
      index = index + needed_padding
    codes.append(code)
    index = index + size
  alignment_needed = NeededPaddingForAlignment(index)
  if alignment_needed:
    codes.append('x' * alignment_needed)
  return struct.Struct(''.join(codes))
