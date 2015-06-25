// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_param_traits.h"

#include "base/strings/stringprintf.h"
#include "content/common/media/video_capture_messages.h"
#include "ipc/ipc_message_utils.h"
#include "media/audio/audio_parameters.h"
#include "media/base/limits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

using media::AudioParameters;
using media::ChannelLayout;
using media::VideoCaptureFormat;

namespace IPC {

void ParamTraits<AudioParameters>::Write(Message* m,
                                         const AudioParameters& p) {
  m->WriteInt(static_cast<int>(p.format()));
  m->WriteInt(static_cast<int>(p.channel_layout()));
  m->WriteInt(p.sample_rate());
  m->WriteInt(p.bits_per_sample());
  m->WriteInt(p.frames_per_buffer());
  m->WriteInt(p.channels());
  m->WriteInt(p.effects());
}

bool ParamTraits<AudioParameters>::Read(const Message* m,
                                        base::PickleIterator* iter,
                                        AudioParameters* r) {
  int format, channel_layout, sample_rate, bits_per_sample,
      frames_per_buffer, channels, effects;

  if (!iter->ReadInt(&format) ||
      !iter->ReadInt(&channel_layout) ||
      !iter->ReadInt(&sample_rate) ||
      !iter->ReadInt(&bits_per_sample) ||
      !iter->ReadInt(&frames_per_buffer) ||
      !iter->ReadInt(&channels) ||
      !iter->ReadInt(&effects)) {
    return false;
  }

  AudioParameters params(static_cast<AudioParameters::Format>(format),
         static_cast<ChannelLayout>(channel_layout), channels,
         sample_rate, bits_per_sample, frames_per_buffer, effects);
  *r = params;
  return r->IsValid();
}

void ParamTraits<AudioParameters>::Log(const AudioParameters& p,
                                       std::string* l) {
  l->append(base::StringPrintf("<AudioParameters>"));
}

void ParamTraits<VideoCaptureFormat>::Write(Message* m,
                                            const VideoCaptureFormat& p) {
  WriteParam(m, p.frame_size);
  WriteParam(m, p.frame_rate);
  WriteParam(m, p.pixel_format);
  WriteParam(m, p.pixel_storage);
}

bool ParamTraits<VideoCaptureFormat>::Read(const Message* m,
                                           base::PickleIterator* iter,
                                           VideoCaptureFormat* r) {
  if (!ReadParam(m, iter, &r->frame_size) ||
      !ReadParam(m, iter, &r->frame_rate) ||
      !ReadParam(m, iter, &r->pixel_format) ||
      !ReadParam(m, iter, &r->pixel_storage)) {
    return false;
  }
  return r->IsValid();
}

void ParamTraits<VideoCaptureFormat>::Log(const VideoCaptureFormat& p,
                                          std::string* l) {
  l->append(
      base::StringPrintf("<VideoCaptureFormat> %s", p.ToString().c_str()));
}

}
