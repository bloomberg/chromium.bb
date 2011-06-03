// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_OMX_OMX_CONFIGURATOR_H_
#define MEDIA_OMX_OMX_CONFIGURATOR_H_

#include <string>

#include "base/basictypes.h"
#include "third_party/openmax/il/OMX_Component.h"
#include "third_party/openmax/il/OMX_Core.h"
#include "third_party/openmax/il/OMX_Video.h"

namespace media {

class OmxConfigurator {
 public:
  enum Codec {
    kCodecNone,
    kCodecH264,
    kCodecMpeg4,
    kCodecH263,
    kCodecVc1,
    kCodecRaw,
  };

  // TODO(jiesun): figure out what other surface formats are.
  enum SurfaceFormat {
    kSurfaceFormatNV21,
    kSurfaceFormatNV21Tiled,
    kSurfaceFormatNV12,
  };

  struct MediaFormatVideoHeader {
    int width;
    int height;
    int stride;     // n/a to compressed stream.
    int frame_rate;
    int bit_rate;   // n/a to raw stream.
    int profile;    // n/a to raw stream.
    int level;      // n/a to raw stream.
    int i_dist;     // i frame distance; >0 if p frame is enabled.
    int p_dist;     // p frame distance; >0 if b frame is enabled.
  };

  struct MediaFormatVideoRaw {
    SurfaceFormat color_space;
  };

  struct MediaFormatVideoH264 {
    int slice_enable;
    int max_ref_frames;
    int num_ref_l0, num_ref_l1;
    int cabac_enable;
    int cabac_init_idc;
    int deblock_enable;
    int frame_mbs_only_flags;
    int mbaff_enable;
    int bdirect_spatial_temporal;
  };

  struct MediaFormatVideoMPEG4 {
    int ac_pred_enable;
    int time_inc_res;
    int slice_enable;
  };

  struct MediaFormat {
    // TODO(jiesun): instead of codec type, we should have media format.
    Codec codec;
    MediaFormatVideoHeader video_header;
    union {
      MediaFormatVideoRaw raw;
      MediaFormatVideoH264 h264;
      MediaFormatVideoMPEG4 mpeg4;
    };
  };

  OmxConfigurator(const MediaFormat& input,
                  const MediaFormat& output)
      : input_format_(input),
        output_format_(output) {
  }

  virtual ~OmxConfigurator() {}

  // Returns the role name for this configuration.
  virtual std::string GetRoleName() const = 0;

  // Called by OmxCodec on the message loop given to it during
  // transition to idle state.
  // OmxCodec reads the current IO port definitions and pass it to this
  // method.
  // Returns true if configuration has completed successfully.
  virtual bool ConfigureIOPorts(
      OMX_COMPONENTTYPE* component,
      OMX_PARAM_PORTDEFINITIONTYPE* input_port_def,
      OMX_PARAM_PORTDEFINITIONTYPE* output_port_def) const = 0;

  const MediaFormat& input_format() const { return input_format_; }
  const MediaFormat& output_format() const { return output_format_; }

 private:
  MediaFormat input_format_;
  MediaFormat output_format_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmxConfigurator);
};

class OmxDecoderConfigurator : public OmxConfigurator {
 public:
  OmxDecoderConfigurator(const MediaFormat& input,
                         const MediaFormat& output)
      : OmxConfigurator(input, output) {
  }

  virtual ~OmxDecoderConfigurator() {}

  virtual std::string GetRoleName() const;

  virtual bool ConfigureIOPorts(
      OMX_COMPONENTTYPE* component,
      OMX_PARAM_PORTDEFINITIONTYPE* input_port_def,
      OMX_PARAM_PORTDEFINITIONTYPE* output_port_def) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmxDecoderConfigurator);
};

class OmxEncoderConfigurator : public OmxConfigurator {
 public:
  OmxEncoderConfigurator(const MediaFormat& input,
                         const MediaFormat& output)
      : OmxConfigurator(input, output) {
  }

  virtual ~OmxEncoderConfigurator() {}

  virtual std::string GetRoleName() const;

  virtual bool ConfigureIOPorts(
      OMX_COMPONENTTYPE* component,
      OMX_PARAM_PORTDEFINITIONTYPE* input_port_def,
      OMX_PARAM_PORTDEFINITIONTYPE* output_port_def) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmxEncoderConfigurator);
};

}  // namespace media

#endif  // MEDIA_OMX_OMX_CONFIGURATOR_H_
