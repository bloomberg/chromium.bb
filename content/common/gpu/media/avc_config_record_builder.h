// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_AVC_CONFIG_RECORD_BUILDER_H_
#define CONTENT_COMMON_GPU_MEDIA_AVC_CONFIG_RECORD_BUILDER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "content/common/content_export.h"

namespace content {

struct H264NALU;
class H264Parser;

// Utility class to build an AVC configuration record given a stream of NALUs
// containing SPS and PPS data.
class CONTENT_EXPORT AVCConfigRecordBuilder {
 public:
  AVCConfigRecordBuilder();
  ~AVCConfigRecordBuilder();

  // Processes the given NALU. If the final AVC decoder configuration record
  // can be built then the NALU is not consumed and the record is returned
  // in |config_record|. Otherwise the NALU is consumed and |config_record|
  // is not modified. Returns true on success, false on failure.
  bool ProcessNALU(H264Parser* parser,
                   const H264NALU& nalu,
                   std::vector<uint8>* config_record);

  int coded_width() const { return coded_width_; }
  int coded_height() const { return coded_height_; }

 private:
  typedef std::vector<scoped_refptr<base::RefCountedBytes> > NALUVector;

  bool ProcessSPS(H264Parser* parser, const H264NALU& nalu);
  bool ProcessPPS(H264Parser* parser, const H264NALU& nalu);
  bool BuildConfigRecord(std::vector<uint8>* config_record);

  // Copies data from |nalus| into |record_buffer|. Returns the number of bytes
  // that were written.
  size_t CopyNALUsToConfigRecord(const NALUVector& nalus, uint8* record_buffer);

  // Data for each SPS.
  NALUVector sps_nalus_;
  // Data for each PPS.
  NALUVector pps_nalus_;
  // The video codec profile stored in the SPS.
  int sps_profile_idc_;
  // The constraint setx flags stored in the SPS.
  int sps_constraint_setx_flag_;
  // The avc level stored in the SPS.
  int sps_level_idc_;
  // The width of the video as enocded in the SPS.
  uint32 coded_width_;
  // The height of the video as enocded in the SPS.
  uint32 coded_height_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_AVC_CONFIG_RECORD_BUILDER_H_
