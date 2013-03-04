// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/avc_config_record_builder.h"

#include <limits>

#include "base/logging.h"
#include "content/common/gpu/media/h264_parser.h"

namespace content {

AVCConfigRecordBuilder::AVCConfigRecordBuilder()
    : sps_profile_idc_(0),
      sps_constraint_setx_flag_(0),
      sps_level_idc_(0),
      coded_width_(0),
      coded_height_(0) {
}

AVCConfigRecordBuilder::~AVCConfigRecordBuilder() {
}

bool AVCConfigRecordBuilder::ProcessNALU(
    H264Parser* parser,
    const H264NALU& nalu,
    std::vector<uint8>* config_record) {
  if (nalu.nal_unit_type == H264NALU::kSPS) {
    return ProcessSPS(parser, nalu);
  } else if (nalu.nal_unit_type == H264NALU::kPPS) {
    return ProcessPPS(parser, nalu);
  } else if (nalu.nal_unit_type >= 1 && nalu.nal_unit_type <= 5) {
    // Ready to build the AVC decoder configuration record once the first slice
    // type is encountered.
    return BuildConfigRecord(config_record);
  }
  // Effectively skip this NALU by returning success.
  return true;
}

bool AVCConfigRecordBuilder::BuildConfigRecord(
    std::vector<uint8>* config_record) {
  // 5 bytes for AVC record header. 1 byte for the number of SPS units.
  // 1 byte for the number of PPS units.
  size_t record_size = 7;
  for (NALUVector::const_iterator it = sps_nalus_.begin();
       it != sps_nalus_.end(); ++it) {
    // Plus 2 bytes to store the SPS size.
    size_t size = (*it)->size() + 2;
    if (std::numeric_limits<size_t>::max() - size <= record_size)
      return false;
    record_size += size;
  }
  for (NALUVector::const_iterator it = pps_nalus_.begin();
       it != pps_nalus_.end(); ++it) {
    // Plus 2 bytes to store the PPS size.
    size_t size = (*it)->size() + 2;
    if (std::numeric_limits<size_t>::max() - size <= record_size)
      return false;
    record_size += size;
  }
  std::vector<uint8> extra_data(record_size);

  // AVC decoder configuration record version.
  extra_data[0] = 0x01;
  // Profile.
  extra_data[1] = sps_profile_idc_ & 0xff;
  // Profile compatibility, must match the byte between profile IDC
  // and level IDC in the SPS.
  extra_data[2] = sps_constraint_setx_flag_;
  // AVC level.
  extra_data[3] = sps_level_idc_ & 0xff;

  // TODO(sail): There's no way to get the NALU length field size from the
  // SPS and PPS data. Just assume 4 for now.
  const size_t kNALULengthFieldSize = 4;

  // The first 6 bits are reserved and must be 1. Last two bits are the
  // NALU field size minus 1.
  extra_data[4] = 0xfc | ((kNALULengthFieldSize - 1) & 0x03);

  // The first 3 bits are reserved and must be 1. Last 5 bits are the
  // number of SPS units.
  extra_data[5] = 0xe0 | (sps_nalus_.size() & 0x1f);
  size_t index = 6;
  if (!sps_nalus_.empty())
    index += CopyNALUsToConfigRecord(sps_nalus_, &extra_data[index]);

  // The number of PPS units.
  extra_data[index++] = pps_nalus_.size() & 0xff;
  if (!pps_nalus_.empty())
    CopyNALUsToConfigRecord(pps_nalus_, &extra_data[index]);

  *config_record = extra_data;
  return true;
}

size_t AVCConfigRecordBuilder::CopyNALUsToConfigRecord(const NALUVector& nalus,
                                                       uint8* record_buffer) {
  size_t index = 0;
  for (NALUVector::const_iterator it = nalus.begin();
       it != nalus.end(); ++it) {
    // High byte of the NALU size.
    record_buffer[index++] = ((*it)->size() >> 8) & 0xff;
    // Low byte of the NALU size.
    record_buffer[index++] = (*it)->size() & 0xff;
    // The NALU data.
    memcpy(record_buffer + index, (*it)->front(), (*it)->size());
    index += (*it)->size();
  }
  return index;
}

bool AVCConfigRecordBuilder::ProcessSPS(H264Parser* parser,
                                        const H264NALU& nalu) {
  int sps_id = 0;
  H264Parser::Result result = parser->ParseSPS(&sps_id);
  if (result != H264Parser::kOk)
    return false;

  std::vector<uint8> bytes(nalu.data, nalu.data + nalu.size);
  sps_nalus_.push_back(base::RefCountedBytes::TakeVector(&bytes));

  const H264SPS* sps = parser->GetSPS(sps_id);

  // Use the last width and height that are encountered.
  coded_width_ = (sps->pic_width_in_mbs_minus1 + 1) * 16;
  if (sps->frame_mbs_only_flag)
    coded_height_ = (sps->pic_height_in_map_units_minus1 + 1) * 16;
  else
    coded_height_ = (sps->pic_height_in_map_units_minus1 + 1) * 32;

  // Use the last video profile and flags that are encountered.
  sps_profile_idc_ = sps->profile_idc;
  sps_constraint_setx_flag_ = sps->constraint_setx_flag;
  // Use the largest AVC level that's encountered.
  sps_level_idc_ = std::max(sps_level_idc_, sps->level_idc);

  return true;
}

bool AVCConfigRecordBuilder::ProcessPPS(H264Parser* parser,
                                        const H264NALU& nalu) {
  int pps_id = 0;
  H264Parser::Result result = parser->ParsePPS(&pps_id);
  if (result != H264Parser::kOk)
    return false;

  std::vector<uint8> bytes(nalu.data, nalu.data + nalu.size);
  pps_nalus_.push_back(base::RefCountedBytes::TakeVector(&bytes));
  return true;
}

}  // namespace content
