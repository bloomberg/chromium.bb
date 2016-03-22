// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_transport_stream_packetizer.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "base/logging.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_descriptor.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_info.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_packetizer.h"

namespace extensions {
namespace {

uint32_t crc32(uint32_t crc, const uint8_t* data, size_t len) {
  static const uint32_t table[256] = {
      0x00000000, 0xb71dc104, 0x6e3b8209, 0xd926430d, 0xdc760413, 0x6b6bc517,
      0xb24d861a, 0x0550471e, 0xb8ed0826, 0x0ff0c922, 0xd6d68a2f, 0x61cb4b2b,
      0x649b0c35, 0xd386cd31, 0x0aa08e3c, 0xbdbd4f38, 0x70db114c, 0xc7c6d048,
      0x1ee09345, 0xa9fd5241, 0xacad155f, 0x1bb0d45b, 0xc2969756, 0x758b5652,
      0xc836196a, 0x7f2bd86e, 0xa60d9b63, 0x11105a67, 0x14401d79, 0xa35ddc7d,
      0x7a7b9f70, 0xcd665e74, 0xe0b62398, 0x57abe29c, 0x8e8da191, 0x39906095,
      0x3cc0278b, 0x8bdde68f, 0x52fba582, 0xe5e66486, 0x585b2bbe, 0xef46eaba,
      0x3660a9b7, 0x817d68b3, 0x842d2fad, 0x3330eea9, 0xea16ada4, 0x5d0b6ca0,
      0x906d32d4, 0x2770f3d0, 0xfe56b0dd, 0x494b71d9, 0x4c1b36c7, 0xfb06f7c3,
      0x2220b4ce, 0x953d75ca, 0x28803af2, 0x9f9dfbf6, 0x46bbb8fb, 0xf1a679ff,
      0xf4f63ee1, 0x43ebffe5, 0x9acdbce8, 0x2dd07dec, 0x77708634, 0xc06d4730,
      0x194b043d, 0xae56c539, 0xab068227, 0x1c1b4323, 0xc53d002e, 0x7220c12a,
      0xcf9d8e12, 0x78804f16, 0xa1a60c1b, 0x16bbcd1f, 0x13eb8a01, 0xa4f64b05,
      0x7dd00808, 0xcacdc90c, 0x07ab9778, 0xb0b6567c, 0x69901571, 0xde8dd475,
      0xdbdd936b, 0x6cc0526f, 0xb5e61162, 0x02fbd066, 0xbf469f5e, 0x085b5e5a,
      0xd17d1d57, 0x6660dc53, 0x63309b4d, 0xd42d5a49, 0x0d0b1944, 0xba16d840,
      0x97c6a5ac, 0x20db64a8, 0xf9fd27a5, 0x4ee0e6a1, 0x4bb0a1bf, 0xfcad60bb,
      0x258b23b6, 0x9296e2b2, 0x2f2bad8a, 0x98366c8e, 0x41102f83, 0xf60dee87,
      0xf35da999, 0x4440689d, 0x9d662b90, 0x2a7bea94, 0xe71db4e0, 0x500075e4,
      0x892636e9, 0x3e3bf7ed, 0x3b6bb0f3, 0x8c7671f7, 0x555032fa, 0xe24df3fe,
      0x5ff0bcc6, 0xe8ed7dc2, 0x31cb3ecf, 0x86d6ffcb, 0x8386b8d5, 0x349b79d1,
      0xedbd3adc, 0x5aa0fbd8, 0xeee00c69, 0x59fdcd6d, 0x80db8e60, 0x37c64f64,
      0x3296087a, 0x858bc97e, 0x5cad8a73, 0xebb04b77, 0x560d044f, 0xe110c54b,
      0x38368646, 0x8f2b4742, 0x8a7b005c, 0x3d66c158, 0xe4408255, 0x535d4351,
      0x9e3b1d25, 0x2926dc21, 0xf0009f2c, 0x471d5e28, 0x424d1936, 0xf550d832,
      0x2c769b3f, 0x9b6b5a3b, 0x26d61503, 0x91cbd407, 0x48ed970a, 0xfff0560e,
      0xfaa01110, 0x4dbdd014, 0x949b9319, 0x2386521d, 0x0e562ff1, 0xb94beef5,
      0x606dadf8, 0xd7706cfc, 0xd2202be2, 0x653deae6, 0xbc1ba9eb, 0x0b0668ef,
      0xb6bb27d7, 0x01a6e6d3, 0xd880a5de, 0x6f9d64da, 0x6acd23c4, 0xddd0e2c0,
      0x04f6a1cd, 0xb3eb60c9, 0x7e8d3ebd, 0xc990ffb9, 0x10b6bcb4, 0xa7ab7db0,
      0xa2fb3aae, 0x15e6fbaa, 0xccc0b8a7, 0x7bdd79a3, 0xc660369b, 0x717df79f,
      0xa85bb492, 0x1f467596, 0x1a163288, 0xad0bf38c, 0x742db081, 0xc3307185,
      0x99908a5d, 0x2e8d4b59, 0xf7ab0854, 0x40b6c950, 0x45e68e4e, 0xf2fb4f4a,
      0x2bdd0c47, 0x9cc0cd43, 0x217d827b, 0x9660437f, 0x4f460072, 0xf85bc176,
      0xfd0b8668, 0x4a16476c, 0x93300461, 0x242dc565, 0xe94b9b11, 0x5e565a15,
      0x87701918, 0x306dd81c, 0x353d9f02, 0x82205e06, 0x5b061d0b, 0xec1bdc0f,
      0x51a69337, 0xe6bb5233, 0x3f9d113e, 0x8880d03a, 0x8dd09724, 0x3acd5620,
      0xe3eb152d, 0x54f6d429, 0x7926a9c5, 0xce3b68c1, 0x171d2bcc, 0xa000eac8,
      0xa550add6, 0x124d6cd2, 0xcb6b2fdf, 0x7c76eedb, 0xc1cba1e3, 0x76d660e7,
      0xaff023ea, 0x18ede2ee, 0x1dbda5f0, 0xaaa064f4, 0x738627f9, 0xc49be6fd,
      0x09fdb889, 0xbee0798d, 0x67c63a80, 0xd0dbfb84, 0xd58bbc9a, 0x62967d9e,
      0xbbb03e93, 0x0cadff97, 0xb110b0af, 0x060d71ab, 0xdf2b32a6, 0x6836f3a2,
      0x6d66b4bc, 0xda7b75b8, 0x035d36b5, 0xb440f7b1};
  for (; len; ++data, --len)
    crc = (crc >> 8) ^ table[(crc & 0xFFu) ^ *data];
  return crc;
}

// Code and parameters related to the Program Specific Information (PSI)
// specification.
namespace psi {

const uint8_t kProgramAssociationTableId = 0x00u;
const uint8_t kProgramMapTableId = 0x02u;

const uint16_t kFirstProgramNumber = 0x0001u;

const size_t kCrcSize = 4u;
const size_t kProgramMapTableElementaryStreamEntryBaseSize = 5u;
const size_t kTableHeaderSize = 3u;

size_t FillInTablePointer(uint8_t* dst, size_t min_size) {
  size_t i = 1u;
  if (i < min_size) {
    std::memset(&dst[i], 0xFF, min_size - i);  // Pointer filler bytes
    i = min_size;
  }
  dst[0] = i - 1u;  // Pointer field
  return i;
}

size_t FillInTableHeaderAndCrc(uint8_t* header_dst,
                               uint8_t* crc_dst,
                               uint8_t table_id) {
  size_t i;
  const uint8_t* const header_end = header_dst + kTableHeaderSize;
  const uint8_t* const crc_end = crc_dst + kCrcSize;
  const size_t section_length = static_cast<size_t>(crc_end - header_end);
  DCHECK_LE(section_length, 1021u);

  // Table header.
  i = 0u;
  header_dst[i++] = table_id;
  header_dst[i++] =
      (0x1u << 7) |  // Section syntax indicator (1 for PAT and PMT)
      (0x0u << 6) |  // Private bit (0 for PAT and PMT)
      (0x3u << 4) |  // Reserved bits (both bits on)
      (0x0u << 2) |  // Section length unused bits (both bits off)
      ((section_length >> 8) & 0x03u);       // Section length (10 bits)
  header_dst[i++] = section_length & 0xFFu;  //
  DCHECK_EQ(kTableHeaderSize, i);

  // CRC.
  uint32_t crc =
      crc32(0xFFFFFFFFu, header_dst, static_cast<size_t>(crc_dst - header_dst));
  i = 0u;
  // Avoid swapping the crc by reversing write order.
  crc_dst[i++] = crc & 0xFFu;
  crc_dst[i++] = (crc >> 8) & 0xFFu;
  crc_dst[i++] = (crc >> 16) & 0xFFu;
  crc_dst[i++] = (crc >> 24) & 0xFFu;
  DCHECK_EQ(kCrcSize, i);
  return i;
}

size_t FillInTableSyntax(uint8_t* dst,
                         uint16_t table_id_extension,
                         uint8_t version_number) {
  size_t i = 0u;
  dst[i++] = table_id_extension >> 8;
  dst[i++] = table_id_extension & 0xFFu;
  dst[i++] = (0x3u << 6) |                      // Reserved bits (both bits on)
             ((version_number & 0x1Fu) << 1) |  // Version number (5 bits)
             (0x1u << 0);                       // Current indicator
  dst[i++] = 0u;                                // Section number
  dst[i++] = 0u;                                // Last section number
  return i;
}

size_t FillInProgramAssociationTableEntry(uint8_t* dst,
                                          uint16_t program_number,
                                          unsigned pmt_packet_id) {
  size_t i = 0u;
  dst[i++] = program_number >> 8;
  dst[i++] = program_number & 0xFFu;
  dst[i++] = (0x7u << 5) |                    // Reserved bits (all 3 bits on)
             ((pmt_packet_id >> 8) & 0x1Fu);  // Packet identifier (13 bits)
  dst[i++] = pmt_packet_id & 0xFFu;           //
  return i;
}

size_t FillInProgramMapTableData(uint8_t* dst, unsigned pcr_packet_id) {
  size_t i = 0u;
  dst[i++] = (0x7u << 5) |                    // Reserved bits (all 3 bits on)
             ((pcr_packet_id >> 8) & 0x1Fu);  // Packet identifier (13 bits)
  dst[i++] = pcr_packet_id & 0xFFu;           //
  dst[i++] = (0xFu << 4) |                    // Reserved bits (all 4 bits on)
             (0x0u << 2) |  // Program info length unused bits (both bits off)
             ((0u >> 8) & 0x3u);  // Program info length (10 bits)
  dst[i++] = 0u & 0xFFu;          //
                                  // No program descriptors
  return i;
}

size_t CalculateElementaryStreamInfoLength(
    const std::vector<WiFiDisplayElementaryStreamDescriptor>& es_descriptors) {
  size_t es_info_length = 0u;
  for (const auto& es_descriptor : es_descriptors)
    es_info_length += es_descriptor.size();
  DCHECK_EQ(0u, es_info_length >> 8);
  return es_info_length;
}

size_t FillInProgramMapTableElementaryStreamEntry(
    uint8_t* dst,
    uint8_t stream_type,
    unsigned es_packet_id,
    size_t es_info_length,
    const std::vector<WiFiDisplayElementaryStreamDescriptor>& es_descriptors) {
  DCHECK_EQ(CalculateElementaryStreamInfoLength(es_descriptors),
            es_info_length);
  DCHECK_EQ(0u, es_info_length >> 10);
  size_t i = 0u;
  dst[i++] = stream_type;
  dst[i++] = (0x7u << 5) |                   // Reserved bits (all 3 bits on)
             ((es_packet_id >> 8) & 0x1Fu);  // Packet identifier (13 bits)
  dst[i++] = es_packet_id & 0xFFu;           //
  dst[i++] = (0xFu << 4) |                   // Reserved bits (all 4 bits on)
             (0x0u << 2) |  // ES info length unused bits (both bits off)
             ((es_info_length >> 8) & 0x3u);  // ES info length (10 bits)
  dst[i++] = es_info_length & 0xFFu;          //
  for (const auto& es_descriptor : es_descriptors) {
    std::memcpy(&dst[i], es_descriptor.data(), es_descriptor.size());
    i += es_descriptor.size();
  }
  return i;
}

}  // namespace psi

// Code and parameters related to the MPEG Transport Stream (MPEG-TS)
// specification.
namespace ts {

const size_t kAdaptationFieldLengthSize = 1u;
const size_t kAdaptationFieldFlagsSize = 1u;
const size_t kPacketHeaderSize = 4u;
const size_t kProgramClockReferenceSize = 6u;

size_t FillInProgramClockReference(uint8_t* dst, const base::TimeTicks& pcr) {
  // Convert to the number of 27 MHz ticks since some epoch.
  const uint64_t us =
      static_cast<uint64_t>((pcr - base::TimeTicks()).InMicroseconds());
  const uint64_t n = 27u * us;
  const uint64_t base = n / 300u;  // 90 kHz
  const uint64_t extension = n % 300u;

  size_t i = 0u;
  dst[i++] = (base >> 25) & 0xFFu;       // Base (33 bits)
  dst[i++] = (base >> 17) & 0xFFu;       //
  dst[i++] = (base >> 9) & 0xFFu;        //
  dst[i++] = (base >> 1) & 0xFFu;        //
  dst[i++] = ((base & 0x01u) << 7) |     //
             (0x3Fu << 1) |              // Reserved bits (all 6 bits on)
             ((extension >> 8) & 0x1u);  // Extension (9 bits)
  dst[i++] = extension & 0xFFu;          //
  DCHECK_EQ(kProgramClockReferenceSize, i);
  return i;
}

size_t FillInAdaptationFieldLengthFromSize(uint8_t* dst, size_t size) {
  size_t i = 0u;
  dst[i++] = size - 1u;
  DCHECK_EQ(kAdaptationFieldLengthSize, i);
  return i;
}

size_t FillInAdaptationFieldFlags(uint8_t* dst,
                                  bool random_access_indicator,
                                  const base::TimeTicks& pcr) {
  size_t i = 0u;
  dst[i++] = (0x0u << 7) |                     // Discontinuity indicator
             (random_access_indicator << 6) |  // Random access indicator
             (0x0u << 5) |              // Elementary stream priority indicator
             ((!pcr.is_null()) << 4) |  // PCR flag
             (0x0u << 3) |              // OPCR flag
             (0x0u << 2) |              // Splicing point flag
             (0x0u << 1) |              // Transport private data flag
             (0x0u << 0);               // Adaptation field extension flag
  DCHECK_EQ(kAdaptationFieldFlagsSize, i);
  return i;
}

size_t FillInAdaptationField(uint8_t* dst,
                             bool random_access_indicator,
                             size_t min_size) {
  // Reserve space for a length.
  size_t i = kAdaptationFieldLengthSize;

  if (random_access_indicator || i < min_size) {
    const base::TimeTicks pcr;
    i += FillInAdaptationFieldFlags(&dst[i], random_access_indicator, pcr);

    if (i < min_size) {
      std::memset(&dst[i], 0xFF, min_size - i);  // Stuffing bytes.
      i = min_size;
    }
  }

  // Fill in a length now that the size is known.
  FillInAdaptationFieldLengthFromSize(dst, i);

  return i;
}

size_t FillInPacketHeader(uint8_t* dst,
                          bool payload_unit_start_indicator,
                          unsigned packet_id,
                          bool adaptation_field_flag,
                          unsigned continuity_counter) {
  size_t i = 0u;
  dst[i++] = 0x47;  // Sync byte ('G')
  dst[i++] =
      (0x0u << 7) |                          // Transport error indicator
      (payload_unit_start_indicator << 6) |  // Payload unit start indicator
      (0x0 << 5) |                           // Transport priority
      ((packet_id >> 8) & 0x1Fu);            // Packet identifier (13 bits)
  dst[i++] = packet_id & 0xFFu;              //
  dst[i++] = (0x0u << 6) |  // Scrambling control (0b00 for not)
             (adaptation_field_flag << 5) |  // Adaptation field flag
             (0x1u << 4) |                   // Payload flag
             (continuity_counter & 0xFu);    // Continuity counter
  DCHECK_EQ(kPacketHeaderSize, i);
  return i;
}

}  // namespace ts

// Code and parameters related to the WiFi Display specification.
namespace widi {

const size_t kUnitHeaderMaxSize = 4u;

// Maximum interval between meta information which includes:
//  * Program Association Table (PAT)
//  * Program Map Table (PMT)
//  * Program Clock Reference (PCR)
const int kMaxMillisecondsBetweenMetaInformation = 100u;

const unsigned kProgramAssociationTablePacketId = 0x0000u;
const unsigned kProgramMapTablePacketId = 0x0100u;
const unsigned kProgramClockReferencePacketId = 0x1000u;
const unsigned kVideoStreamPacketId = 0x1011u;
const unsigned kFirstAudioStreamPacketId = 0x1100u;

size_t FillInUnitHeader(uint8_t* dst,
                        const WiFiDisplayElementaryStreamInfo& stream_info) {
  size_t i = 0u;

  if (stream_info.type() == WiFiDisplayElementaryStreamInfo::AUDIO_LPCM) {
    // Convert an LPCM audio stream descriptor to an LPCM unit header.
    if (const auto* lpcm_descriptor =
            stream_info.FindDescriptor<
                WiFiDisplayElementaryStreamDescriptor::LPCMAudioStream>()) {
      dst[i++] = 0xA0u;  // Sub stream ID (0th sub stream)
      dst[i++] = WiFiDisplayTransportStreamPacketizer::LPCM::kFramesPerUnit;
      dst[i++] = ((0x00u << 1) |  // Reserved (all 7 bits off)
                  (lpcm_descriptor->emphasis_flag() << 0));
      dst[i++] = ((lpcm_descriptor->bits_per_sample() << 6) |
                  (lpcm_descriptor->sampling_frequency() << 3) |
                  (lpcm_descriptor->number_of_channels() << 0));
    }
  }

  DCHECK_LE(i, kUnitHeaderMaxSize);
  return i;
}

}  // namespace widi

}  // namespace

WiFiDisplayTransportStreamPacket::WiFiDisplayTransportStreamPacket(
    const uint8_t* header_data,
    size_t header_size)
    : header_(header_data, header_size),
      payload_(header_.end(), 0u),
      filler_(kPacketSize - header_size) {}

WiFiDisplayTransportStreamPacket::WiFiDisplayTransportStreamPacket(
    const uint8_t* header_data,
    size_t header_size,
    const uint8_t* payload_data)
    : header_(header_data, header_size),
      payload_(payload_data, kPacketSize - header_size),
      filler_(0u) {}

struct WiFiDisplayTransportStreamPacketizer::ElementaryStreamState {
  ElementaryStreamState(WiFiDisplayElementaryStreamInfo info,
                        uint16_t packet_id,
                        uint8_t stream_id)
      : info(std::move(info)),
        info_length(
            psi::CalculateElementaryStreamInfoLength(this->info.descriptors())),
        packet_id(packet_id),
        stream_id(stream_id) {}

  WiFiDisplayElementaryStreamInfo info;
  uint8_t info_length;
  struct {
    uint8_t continuity = 0u;
  } counters;
  uint16_t packet_id;
  uint8_t stream_id;
};

WiFiDisplayTransportStreamPacketizer::WiFiDisplayTransportStreamPacketizer(
    const base::TimeDelta& delay_for_unit_time_stamps,
    std::vector<WiFiDisplayElementaryStreamInfo> stream_infos)
    : delay_for_unit_time_stamps_(delay_for_unit_time_stamps) {
  std::memset(&counters_, 0x00, sizeof(counters_));
  if (!stream_infos.empty())
    CHECK(SetElementaryStreams(std::move(stream_infos)));
}

WiFiDisplayTransportStreamPacketizer::~WiFiDisplayTransportStreamPacketizer() {}

bool WiFiDisplayTransportStreamPacketizer::EncodeElementaryStreamUnit(
    unsigned stream_index,
    const uint8_t* unit_data,
    size_t unit_size,
    bool random_access,
    base::TimeTicks pts,
    base::TimeTicks dts,
    bool flush) {
  DCHECK(CalledOnValidThread());
  DCHECK_LT(stream_index, stream_states_.size());
  ElementaryStreamState& stream_state = stream_states_[stream_index];

  if (program_clock_reference_.is_null() ||
      base::TimeTicks::Now() - program_clock_reference_ >
          base::TimeDelta::FromMilliseconds(
              widi::kMaxMillisecondsBetweenMetaInformation) /
              2) {
    if (!EncodeMetaInformation(false))
      return false;
  }

  uint8_t unit_header_data[widi::kUnitHeaderMaxSize];
  const size_t unit_header_size =
      widi::FillInUnitHeader(unit_header_data, stream_state.info);

  UpdateDelayForUnitTimeStamps(pts, dts);
  NormalizeUnitTimeStamps(&pts, &dts);

  WiFiDisplayElementaryStreamPacketizer elementary_stream_packetizer;
  WiFiDisplayElementaryStreamPacket elementary_stream_packet =
      elementary_stream_packetizer.EncodeElementaryStreamUnit(
          stream_state.stream_id, unit_header_data, unit_header_size, unit_data,
          unit_size, pts, dts);

  size_t adaptation_field_min_size = 0u;
  uint8_t header_data[WiFiDisplayTransportStreamPacket::kPacketSize];
  bool is_payload_unit_end;
  bool is_payload_unit_start = true;
  size_t remaining_unit_size = elementary_stream_packet.unit().size();
  do {
    // Fill in headers and an adaptation field:
    //  * Transport stream packet header
    //  * Transport stream adaptation field
    //    (only for the first and/or the last packet):
    //     - for the first packet to hold flags
    //     - for the last packet to hold padding
    //  * PES packet header (only for the first packet):
    //     - PES packet header base
    //     - Optional PES header base
    //     - Optional PES header optional fields:
    //        - Presentation time stamp
    //        - Decoding time stamp
    bool adaptation_field_flag = false;
    size_t header_min_size;
    if (is_payload_unit_start || is_payload_unit_end) {
      header_min_size = ts::kPacketHeaderSize;
      if (is_payload_unit_start) {
        header_min_size += elementary_stream_packet.header().size() +
                           elementary_stream_packet.unit_header().size();
      }
      adaptation_field_min_size =
          std::max(
              WiFiDisplayTransportStreamPacket::kPacketSize - header_min_size,
              remaining_unit_size) -
          remaining_unit_size;
      adaptation_field_flag = adaptation_field_min_size > 0 ||
                              (is_payload_unit_start && random_access);
    }
    size_t i = 0u;
    i += ts::FillInPacketHeader(&header_data[i], is_payload_unit_start,
                                stream_state.packet_id, adaptation_field_flag,
                                stream_state.counters.continuity++);
    if (is_payload_unit_start) {
      size_t adaptation_field_size = adaptation_field_min_size;
      if (adaptation_field_flag) {
        adaptation_field_size = ts::FillInAdaptationField(
            &header_data[i], random_access, adaptation_field_min_size);
        i += adaptation_field_size;
        DCHECK_GE(adaptation_field_size, adaptation_field_min_size);
      }
      std::memcpy(&header_data[i], elementary_stream_packet.header().data(),
                  elementary_stream_packet.header().size());
      i += elementary_stream_packet.header().size();
      std::memcpy(&header_data[i],
                  elementary_stream_packet.unit_header().data(),
                  elementary_stream_packet.unit_header().size());
      i += elementary_stream_packet.unit_header().size();
      DCHECK_EQ(header_min_size + adaptation_field_size, i);
    } else if (is_payload_unit_end) {
      if (adaptation_field_flag) {
        // Fill in an adaptation field only for padding.
        i += ts::FillInAdaptationField(&header_data[i], false,
                                       adaptation_field_min_size);
      }
      DCHECK_EQ(header_min_size + adaptation_field_min_size, i);
    }

    // Delegate the packet.
    WiFiDisplayTransportStreamPacket packet(
        header_data, i,
        elementary_stream_packet.unit().end() - remaining_unit_size);
    DCHECK_LE(packet.payload().size(), remaining_unit_size);
    remaining_unit_size -= packet.payload().size();
    if (!OnPacketizedTransportStreamPacket(
            packet, flush && remaining_unit_size == 0u)) {
      return false;
    }

    // Prepare for the next packet.
    is_payload_unit_end =
        remaining_unit_size <=
        WiFiDisplayTransportStreamPacket::kPacketSize - ts::kPacketHeaderSize;
    is_payload_unit_start = false;
  } while (remaining_unit_size > 0u);

  DCHECK_EQ(0u, remaining_unit_size);

  return true;
}

bool WiFiDisplayTransportStreamPacketizer::EncodeMetaInformation(bool flush) {
  DCHECK(CalledOnValidThread());

  return (EncodeProgramAssociationTable(false) &&
          EncodeProgramMapTables(false) && EncodeProgramClockReference(flush));
}

bool WiFiDisplayTransportStreamPacketizer::EncodeProgramAssociationTable(
    bool flush) {
  DCHECK(CalledOnValidThread());

  const uint16_t transport_stream_id = 0x0001u;

  uint8_t header_data[WiFiDisplayTransportStreamPacket::kPacketSize];
  size_t i = 0u;

  // Fill in a packet header.
  i += ts::FillInPacketHeader(&header_data[i], true,
                              widi::kProgramAssociationTablePacketId, false,
                              counters_.program_association_table_continuity++);

  // Fill in a minimal table pointer.
  i += psi::FillInTablePointer(&header_data[i], 0u);

  // Reserve space for a table header.
  const size_t table_header_index = i;
  i += psi::kTableHeaderSize;

  // Fill in a table syntax.
  const uint8_t version_number = 0u;
  i += psi::FillInTableSyntax(&header_data[i], transport_stream_id,
                              version_number);

  // Fill in program association table data.
  i += psi::FillInProgramAssociationTableEntry(&header_data[i],
                                               psi::kFirstProgramNumber,
                                               widi::kProgramMapTablePacketId);

  // Fill in a table header and a CRC now that the table size is known.
  i += psi::FillInTableHeaderAndCrc(&header_data[table_header_index],
                                    &header_data[i],
                                    psi::kProgramAssociationTableId);

  // Delegate the packet.
  return OnPacketizedTransportStreamPacket(
      WiFiDisplayTransportStreamPacket(header_data, i), flush);
}

bool WiFiDisplayTransportStreamPacketizer::EncodeProgramClockReference(
    bool flush) {
  DCHECK(CalledOnValidThread());

  program_clock_reference_ = base::TimeTicks::Now();

  uint8_t header_data[ts::kPacketHeaderSize + ts::kAdaptationFieldLengthSize +
                      ts::kAdaptationFieldFlagsSize +
                      ts::kProgramClockReferenceSize];
  size_t i = 0u;

  // Fill in a packet header.
  i += ts::FillInPacketHeader(&header_data[i], true,
                              widi::kProgramClockReferencePacketId, true,
                              counters_.program_clock_reference_continuity++);

  // Fill in an adaptation field.
  i += ts::FillInAdaptationFieldLengthFromSize(
      &header_data[i], WiFiDisplayTransportStreamPacket::kPacketSize - i);
  i += ts::FillInAdaptationFieldFlags(&header_data[i], false,
                                      program_clock_reference_);
  i += ts::FillInProgramClockReference(&header_data[i],
                                       program_clock_reference_);

  DCHECK_EQ(std::end(header_data), header_data + i);

  // Delegate the packet.
  return OnPacketizedTransportStreamPacket(
      WiFiDisplayTransportStreamPacket(header_data, i), flush);
}

bool WiFiDisplayTransportStreamPacketizer::EncodeProgramMapTables(bool flush) {
  DCHECK(CalledOnValidThread());
  DCHECK(!stream_states_.empty());

  const uint16_t program_number = psi::kFirstProgramNumber;

  uint8_t header_data[WiFiDisplayTransportStreamPacket::kPacketSize];
  size_t i = 0u;

  // Fill in a packet header.
  i += ts::FillInPacketHeader(&header_data[i], true,
                              widi::kProgramMapTablePacketId, false,
                              counters_.program_map_table_continuity++);

  // Fill in a minimal table pointer.
  i += psi::FillInTablePointer(&header_data[i], 0u);

  // Reserve space for a table header.
  const size_t table_header_index = i;
  i += psi::kTableHeaderSize;

  // Fill in a table syntax.
  i += psi::FillInTableSyntax(&header_data[i], program_number,
                              counters_.program_map_table_version);

  // Fill in program map table data.
  i += psi::FillInProgramMapTableData(&header_data[i],
                                      widi::kProgramClockReferencePacketId);
  for (const auto& stream_state : stream_states_) {
    DCHECK_LE(i + psi::kProgramMapTableElementaryStreamEntryBaseSize +
                  stream_state.info_length + psi::kCrcSize,
              WiFiDisplayTransportStreamPacket::kPacketSize);
    i += psi::FillInProgramMapTableElementaryStreamEntry(
        &header_data[i], stream_state.info.type(), stream_state.packet_id,
        stream_state.info_length, stream_state.info.descriptors());
  }

  // Fill in a table header and a CRC now that the table size is known.
  i += psi::FillInTableHeaderAndCrc(&header_data[table_header_index],
                                    &header_data[i], psi::kProgramMapTableId);

  // Delegate the packet.
  return OnPacketizedTransportStreamPacket(
      WiFiDisplayTransportStreamPacket(header_data, i), flush);
}

void WiFiDisplayTransportStreamPacketizer::NormalizeUnitTimeStamps(
    base::TimeTicks* pts,
    base::TimeTicks* dts) const {
  DCHECK(CalledOnValidThread());

  // Normalize a presentation time stamp.
  if (!pts || pts->is_null())
    return;
  *pts += delay_for_unit_time_stamps_;
  DCHECK_LE(program_clock_reference_, *pts);

  // Normalize a decoding time stamp.
  if (!dts || dts->is_null())
    return;
  *dts += delay_for_unit_time_stamps_;
  DCHECK_LE(program_clock_reference_, *dts);
  DCHECK_LE(*dts, *pts);
}

bool WiFiDisplayTransportStreamPacketizer::SetElementaryStreams(
    std::vector<WiFiDisplayElementaryStreamInfo> stream_infos) {
  DCHECK(CalledOnValidThread());

  std::vector<ElementaryStreamState> new_stream_states;
  new_stream_states.reserve(stream_infos.size());

  uint8_t audio_stream_id =
      WiFiDisplayElementaryStreamPacketizer::kFirstAudioStreamId;
  uint16_t audio_stream_packet_id = widi::kFirstAudioStreamPacketId;
  uint8_t private_stream_1_id =
      WiFiDisplayElementaryStreamPacketizer::kPrivateStream1Id;
  uint16_t video_stream_packet_id = widi::kVideoStreamPacketId;

  for (auto& stream_info : stream_infos) {
    uint16_t packet_id;
    uint8_t stream_id;

    switch (stream_info.type()) {
      case AUDIO_AAC:
        packet_id = audio_stream_packet_id++;
        stream_id = audio_stream_id++;
        break;
      case AUDIO_AC3:
      case AUDIO_LPCM:
        if (private_stream_1_id !=
            WiFiDisplayElementaryStreamPacketizer::kPrivateStream1Id) {
          return false;
        }
        packet_id = audio_stream_packet_id++;
        stream_id = private_stream_1_id++;
        break;
      case VIDEO_H264:
        if (video_stream_packet_id != widi::kVideoStreamPacketId)
          return false;
        packet_id = video_stream_packet_id++;
        stream_id = WiFiDisplayElementaryStreamPacketizer::kFirstVideoStreamId;
        break;
    }

    new_stream_states.emplace_back(std::move(stream_info), packet_id,
                                   stream_id);
  }

  // If there are no previous states, there is no previous program map table
  // to change, either. This ensures that the first encoded program map table
  // has version 0.
  if (!stream_states_.empty())
    ++counters_.program_map_table_version;

  stream_states_.swap(new_stream_states);

  return true;
}

void WiFiDisplayTransportStreamPacketizer::UpdateDelayForUnitTimeStamps(
    const base::TimeTicks& pts,
    const base::TimeTicks& dts) {
  DCHECK(CalledOnValidThread());

  if (pts.is_null())
    return;

  const base::TimeTicks now = base::TimeTicks::Now();
  DCHECK_LE(program_clock_reference_, now);

  // Ensure that delayed time stamps are greater than or equal to now.
  const base::TimeTicks ts_min =
      (dts.is_null() ? pts : dts) + delay_for_unit_time_stamps_;
  if (now > ts_min) {
    const base::TimeDelta error = now - ts_min;
    delay_for_unit_time_stamps_ += 2 * error;
  }
}

}  // namespace extensions
