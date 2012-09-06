// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "mkvmuxerutil.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <limits>
#include <new>

#include "mkvwriter.hpp"
#include "webmids.hpp"

namespace mkvmuxer {

int32 GetCodedUIntSize(uint64 value) {
  if (value < 0x000000000000007FULL)
    return 1;
  else if (value < 0x0000000000003FFFULL)
    return 2;
  else if (value < 0x00000000001FFFFFULL)
    return 3;
  else if (value < 0x000000000FFFFFFFULL)
    return 4;
  else if (value < 0x00000007FFFFFFFFULL)
    return 5;
  else if (value < 0x000003FFFFFFFFFFULL)
    return 6;
  else if (value < 0x0001FFFFFFFFFFFFULL)
    return 7;
  return 8;
}

int32 GetUIntSize(uint64 value) {
  if (value < 0x0000000000000100ULL)
    return 1;
  else if (value < 0x0000000000010000ULL)
    return 2;
  else if (value < 0x0000000001000000ULL)
    return 3;
  else if (value < 0x0000000100000000ULL)
    return 4;
  else if (value < 0x0000010000000000ULL)
    return 5;
  else if (value < 0x0001000000000000ULL)
    return 6;
  else if (value < 0x0100000000000000ULL)
    return 7;
  return 8;
}

uint64 EbmlMasterElementSize(uint64 type, uint64 value) {
  // Size of EBML ID
  int32 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += GetCodedUIntSize(value);

  return ebml_size;
}

uint64 EbmlElementSize(uint64 type, uint64 value) {
  // Size of EBML ID
  int32 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += GetUIntSize(value);

  // Size of Datasize
  ebml_size++;

  return ebml_size;
}

uint64 EbmlElementSize(uint64 type, float value) {
  // Size of EBML ID
  uint64 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += sizeof(value);

  // Size of Datasize
  ebml_size++;

  return ebml_size;
}

uint64 EbmlElementSize(uint64 type, const char* value) {
  if (!value)
    return 0;

  // Size of EBML ID
  uint64 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += strlen(value);

  // Size of Datasize
  ebml_size++;

  return ebml_size;
}

uint64 EbmlElementSize(uint64 type, const uint8* value, uint64 size) {
  if (!value)
    return 0;

  // Size of EBML ID
  uint64 ebml_size = GetUIntSize(type);

  // Datasize
  ebml_size += size;

  // Size of Datasize
  ebml_size += GetCodedUIntSize(size);

  return ebml_size;
}

int32 SerializeInt(IMkvWriter* writer, int64 value, int32 size) {
  if (!writer || size < 1 || size > 8)
    return -1;

  for (int32 i = 1; i <= size; ++i) {
    const int32 byte_count = size - i;
    const int32 bit_count = byte_count * 8;

    const int64 bb = value >> bit_count;
    const uint8 b = static_cast<uint8>(bb);

    const int32 status = writer->Write(&b, 1);

    if (status < 0)
      return status;
  }

  return 0;
}

int32 SerializeFloat(IMkvWriter* writer, float f) {
  if (!writer)
    return -1;

  const uint32& val = reinterpret_cast<const uint32&>(f);

  for (int32 i = 1; i <= 4; ++i) {
    const int32 byte_count = 4 - i;
    const int32 bit_count = byte_count * 8;

    const uint32 bb = val >> bit_count;
    const uint8 b = static_cast<uint8>(bb);

    const int32 status = writer->Write(&b, 1);

    if (status < 0)
      return status;
  }

  return 0;
}

int32 WriteUInt(IMkvWriter* writer, uint64 value) {
  if (!writer)
    return -1;

  int32 size = GetCodedUIntSize(value);

  return WriteUIntSize(writer, value, size);
}

int32 WriteUIntSize(IMkvWriter* writer, uint64 value, int32 size) {
  if (!writer || size < 0 || size > 8)
    return -1;

  if (size > 0) {
    const uint64 bit = 1LL << (size * 7);

    if (value > (bit - 2))
      return -1;

    value |= bit;
  } else {
    size = 1;
    int64 bit;

    for (;;) {
      bit = 1LL << (size * 7);
      const uint64 max = bit - 2;

      if (value <= max)
        break;

      ++size;
    }

    if (size > 8)
      return false;

    value |= bit;
  }

  return SerializeInt(writer, value, size);
}

int32 WriteID(IMkvWriter* writer, uint64 type) {
  if (!writer)
    return -1;

  writer->ElementStartNotify(type, writer->Position());

  const int32 size = GetUIntSize(type);

  return SerializeInt(writer, type, size);
}

bool WriteEbmlMasterElement(IMkvWriter* writer, uint64 type, uint64 size) {
  if (!writer)
    return false;

  if (WriteID(writer, type))
    return false;

  if (WriteUInt(writer, size))
    return false;

  return true;
}

bool WriteEbmlElement(IMkvWriter* writer, uint64 type, uint64 value) {
  if (!writer)
    return false;

  if (WriteID(writer, type))
    return false;

  const uint64 size = GetUIntSize(value);
  if (WriteUInt(writer, size))
    return false;

  if (SerializeInt(writer, value, static_cast<int32>(size)))
    return false;

  return true;
}

bool WriteEbmlElement(IMkvWriter* writer, uint64 type, float value) {
  if (!writer)
    return false;

  if (WriteID(writer, type))
    return false;

  if (WriteUInt(writer, 4))
    return false;

  if (SerializeFloat(writer, value))
    return false;

  return true;
}

bool WriteEbmlElement(IMkvWriter* writer, uint64 type, const char* value) {
  if (!writer || !value)
    return false;

  if (WriteID(writer, type))
    return false;

  const int32 length = strlen(value);
  if (WriteUInt(writer, length))
    return false;

  if (writer->Write(value, length))
    return false;

  return true;
}

bool WriteEbmlElement(IMkvWriter* writer,
                      uint64 type,
                      const uint8* value,
                      uint64 size) {
  if (!writer || !value || size < 1)
    return false;

  if (WriteID(writer, type))
    return false;

  if (WriteUInt(writer, size))
    return false;

  if (writer->Write(value, static_cast<uint32>(size)))
    return false;

  return true;
}

uint64 WriteSimpleBlock(IMkvWriter* writer,
                        const uint8* data,
                        uint64 length,
                        uint64 track_number,
                        int64 timecode,
                        uint64 is_key) {
  if (!writer)
    return false;

  if (!data || length < 1)
    return false;

  //  Here we only permit track number values to be no greater than
  //  126, which the largest value we can store having a Matroska
  //  integer representation of only 1 byte.

  if (track_number < 1 || track_number > 126)
    return false;

  //  Technically the timestamp for a block can be less than the
  //  timestamp for the cluster itself (remember that block timestamp
  //  is a signed, 16-bit integer).  However, as a simplification we
  //  only permit non-negative cluster-relative timestamps for blocks.

  if (timecode < 0 || timecode > std::numeric_limits<int16>::max())
    return false;

  if (WriteID(writer, kMkvSimpleBlock))
    return 0;

  const int32 size = static_cast<int32>(length) + 4;
  if (WriteUInt(writer, size))
    return 0;

  if (WriteUInt(writer, static_cast<uint64>(track_number)))
    return 0;

  if (SerializeInt(writer, timecode, 2))
    return 0;

  uint64 flags = 0;
  if (is_key)
    flags |= 0x80;

  if (SerializeInt(writer, flags, 1))
    return 0;

  if (writer->Write(data, static_cast<uint32>(length)))
    return 0;

  const uint64 element_size =
    GetUIntSize(kMkvSimpleBlock) + GetCodedUIntSize(size) + 4 + length;

  return element_size;
}

// We must write the metadata (key)frame as a BlockGroup element,
// because we need to specify a duration for the frame.  The
// BlockGroup element comprises the frame itself and its duration,
// and is laid out as follows:
//
//   BlockGroup tag
//   BlockGroup size
//     Block tag
//     Block size
//     (the frame is the block payload)
//     Duration tag
//     Duration size
//     (duration payload)
//
uint64 WriteMetadataBlock(IMkvWriter* writer,
                          const uint8* data,
                          uint64 length,
                          uint64 track_number,
                          int64 timecode,
                          uint64 duration) {
  // We don't backtrack when writing to the stream, so we must
  // pre-compute the BlockGroup size, by summing the sizes of each
  // sub-element (the block and the duration).

  // We use a single byte for the track number of the block, which
  // means the block header is exactly 4 bytes.

  const uint64 block_payload_size = 4 + length;
  const int32 block_size = GetCodedUIntSize(block_payload_size);
  const uint64 block_elem_size = 1 + block_size + block_payload_size;

  const int32 duration_payload_size = GetUIntSize(duration);
  const int32 duration_size = GetCodedUIntSize(duration_payload_size);
  const uint64 duration_elem_size = 1 + duration_size + duration_payload_size;

  const uint64 blockg_payload_size = block_elem_size + duration_elem_size;
  const int32 blockg_size = GetCodedUIntSize(blockg_payload_size);
  const uint64 blockg_elem_size = 1 + blockg_size + blockg_payload_size;

  if (WriteID(writer, kMkvBlockGroup))  // 1-byte ID size
    return 0;

  if (WriteUInt(writer, blockg_payload_size))
    return 0;

  //  Write Block element

  if (WriteID(writer, kMkvBlock))  // 1-byte ID size
    return 0;

  if (WriteUInt(writer, block_payload_size))
    return 0;

  // Byte 1 of 4

  if (WriteUInt(writer, track_number))
    return 0;

  // Bytes 2 & 3 of 4

  if (SerializeInt(writer, timecode, 2))
    return 0;

  // Byte 4 of 4

  const uint64 flags = 0;

  if (SerializeInt(writer, flags, 1))
    return 0;

  // Now write the actual frame (of metadata)

  if (writer->Write(data, static_cast<uint32>(length)))
    return 0;

  // Write Duration element

  if (WriteID(writer, kMkvDuration))  // 1-byte ID size
    return 0;

  if (WriteUInt(writer, duration_payload_size))
    return 0;

  if (SerializeInt(writer, duration, duration_payload_size))
    return 0;

  // Note that we don't write a reference time as part of the block
  // group; no reference time(s) indicates that this block is a
  // keyframe.  (Unlike the case for a SimpleBlock element, the header
  // bits of the Block sub-element of a BlockGroup element do not
  // indicate keyframe status.  The keyframe status is inferred from
  // the absence of reference time sub-elements.)

  return blockg_elem_size;
}

uint64 WriteVoidElement(IMkvWriter* writer, uint64 size) {
  if (!writer)
    return false;

  // Subtract one for the void ID and the coded size.
  uint64 void_entry_size = size - 1 - GetCodedUIntSize(size-1);
  uint64 void_size = EbmlMasterElementSize(kMkvVoid, void_entry_size) +
                     void_entry_size;

  if (void_size != size)
    return 0;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return 0;

  if (WriteID(writer, kMkvVoid))
    return 0;

  if (WriteUInt(writer, void_entry_size))
    return 0;

  const uint8 value = 0;
  for (int32 i = 0; i < static_cast<int32>(void_entry_size); ++i) {
    if (writer->Write(&value, 1))
      return 0;
  }

  const int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(void_size))
    return 0;

  return void_size;
}

void GetVersion(int32* major, int32* minor, int32* build, int32* revision) {
  *major = 0;
  *minor = 1;
  *build = 0;
  *revision = 0;
}

}  // namespace mkvmuxer
