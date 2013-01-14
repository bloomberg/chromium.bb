// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/audio_param_traits.h"

#include "base/stringprintf.h"
#include "media/audio/audio_parameters.h"

using media::AudioParameters;
using media::ChannelLayout;

namespace IPC {

void ParamTraits<AudioParameters>::Write(Message* m,
                                         const AudioParameters& p) {
  m->WriteInt(static_cast<int>(p.format()));
  m->WriteInt(static_cast<int>(p.channel_layout()));
  m->WriteInt(p.sample_rate());
  m->WriteInt(p.bits_per_sample());
  m->WriteInt(p.frames_per_buffer());
  m->WriteInt(p.channels());
}

bool ParamTraits<AudioParameters>::Read(const Message* m,
                                        PickleIterator* iter,
                                        AudioParameters* r) {
  int format, channel_layout, sample_rate, bits_per_sample,
      frames_per_buffer, channels;

  if (!m->ReadInt(iter, &format) ||
      !m->ReadInt(iter, &channel_layout) ||
      !m->ReadInt(iter, &sample_rate) ||
      !m->ReadInt(iter, &bits_per_sample) ||
      !m->ReadInt(iter, &frames_per_buffer) ||
      !m->ReadInt(iter, &channels))
    return false;
  r->Reset(static_cast<AudioParameters::Format>(format),
           static_cast<ChannelLayout>(channel_layout),
           sample_rate, bits_per_sample, frames_per_buffer);
  if (!r->IsValid())
    return false;
  return true;
}

void ParamTraits<AudioParameters>::Log(const AudioParameters& p,
                                       std::string* l) {
  l->append(base::StringPrintf("<AudioParameters>"));
}

}
