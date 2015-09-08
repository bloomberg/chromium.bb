// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_HEVC_H_
#define MEDIA_FORMATS_MP4_HEVC_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {

struct SubsampleEntry;

namespace mp4 {

struct MEDIA_EXPORT HEVCDecoderConfigurationRecord : Box {
  DECLARE_BOX_METHODS(HEVCDecoderConfigurationRecord);

  // Parses HEVCDecoderConfigurationRecord data encoded in |data|.
  // Note: This method is intended to parse data outside the MP4StreamParser
  //       context and therefore the box header is not expected to be present
  //       in |data|.
  // Returns true if |data| was successfully parsed.
  bool Parse(const uint8* data, int data_size);

  uint8 configurationVersion;
  uint8 general_profile_space;
  uint8 general_tier_flag;
  uint8 general_profile_idc;
  uint32 general_profile_compatibility_flags;
  uint64 general_constraint_indicator_flags;
  uint8 general_level_idc;
  uint16 min_spatial_segmentation_idc;
  uint8 parallelismType;
  uint8 chromaFormat;
  uint8 bitDepthLumaMinus8;
  uint8 bitDepthChromaMinus8;
  uint16 avgFrameRate;
  uint8 constantFrameRate;
  uint8 numTemporalLayers;
  uint8 temporalIdNested;
  uint8 lengthSizeMinusOne;
  uint8 numOfArrays;

  typedef std::vector<uint8> HVCCNALUnit;
  struct HVCCNALArray {
    HVCCNALArray();
    ~HVCCNALArray();
    uint8 first_byte;
    std::vector<HVCCNALUnit> units;
  };
  std::vector<HVCCNALArray> arrays;

 private:
  bool ParseInternal(BufferReader* reader,
                     const scoped_refptr<MediaLog>& media_log);
};

class MEDIA_EXPORT HEVC {
 public:
  static bool ConvertConfigToAnnexB(
      const HEVCDecoderConfigurationRecord& hevc_config,
      std::vector<uint8>* buffer);

  static bool InsertParamSetsAnnexB(
      const HEVCDecoderConfigurationRecord& hevc_config,
      std::vector<uint8>* buffer,
      std::vector<SubsampleEntry>* subsamples);

  // Verifies that the contents of |buffer| conform to
  // Section 7.4.2.4.4 of ISO/IEC 23008-2.
  // |subsamples| contains the information about what parts of the buffer are
  // encrypted and which parts are clear.
  // Returns true if |buffer| contains conformant Annex B data
  // TODO(servolk): Remove the std::vector version when we can use,
  // C++11's std::vector<T>::data() method.
  static bool IsValidAnnexB(const std::vector<uint8>& buffer,
                            const std::vector<SubsampleEntry>& subsamples);
  static bool IsValidAnnexB(const uint8* buffer, size_t size,
                            const std::vector<SubsampleEntry>& subsamples);
};

class HEVCBitstreamConverter : public BitstreamConverter {
 public:
  explicit HEVCBitstreamConverter(
      scoped_ptr<HEVCDecoderConfigurationRecord> hevc_config);

  // BitstreamConverter interface
  bool ConvertFrame(std::vector<uint8>* frame_buf,
                    bool is_keyframe,
                    std::vector<SubsampleEntry>* subsamples) const override;
 private:
  ~HEVCBitstreamConverter() override;
  scoped_ptr<HEVCDecoderConfigurationRecord> hevc_config_;
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_HEVC_H_
