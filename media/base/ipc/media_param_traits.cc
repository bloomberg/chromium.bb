// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ipc/media_param_traits.h"

#include "base/strings/stringprintf.h"
#include "ipc/ipc_message_utils.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_point.h"
#include "media/base/encryption_scheme.h"
#include "media/base/limits.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

using media::AudioParameters;
using media::AudioLatency;
using media::ChannelLayout;

namespace IPC {

void ParamTraits<AudioParameters>::GetSize(base::PickleSizer* s,
                                           const AudioParameters& p) {
  GetParamSize(s, p.format());
  GetParamSize(s, p.channel_layout());
  GetParamSize(s, p.sample_rate());
  GetParamSize(s, p.bits_per_sample());
  GetParamSize(s, p.frames_per_buffer());
  GetParamSize(s, p.channels());
  GetParamSize(s, p.effects());
  GetParamSize(s, p.mic_positions());
  GetParamSize(s, p.latency_tag());
}

void ParamTraits<AudioParameters>::Write(base::Pickle* m,
                                         const AudioParameters& p) {
  WriteParam(m, p.format());
  WriteParam(m, p.channel_layout());
  WriteParam(m, p.sample_rate());
  WriteParam(m, p.bits_per_sample());
  WriteParam(m, p.frames_per_buffer());
  WriteParam(m, p.channels());
  WriteParam(m, p.effects());
  WriteParam(m, p.mic_positions());
  WriteParam(m, p.latency_tag());
}

bool ParamTraits<AudioParameters>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        AudioParameters* r) {
  AudioParameters::Format format;
  ChannelLayout channel_layout;
  int sample_rate, bits_per_sample, frames_per_buffer, channels, effects;
  std::vector<media::Point> mic_positions;
  AudioLatency::LatencyType latency_tag;

  if (!ReadParam(m, iter, &format) || !ReadParam(m, iter, &channel_layout) ||
      !ReadParam(m, iter, &sample_rate) ||
      !ReadParam(m, iter, &bits_per_sample) ||
      !ReadParam(m, iter, &frames_per_buffer) ||
      !ReadParam(m, iter, &channels) || !ReadParam(m, iter, &effects) ||
      !ReadParam(m, iter, &mic_positions) ||
      !ReadParam(m, iter, &latency_tag)) {
    return false;
  }

  AudioParameters params(format, channel_layout, sample_rate, bits_per_sample,
                         frames_per_buffer);
  params.set_channels_for_discrete(channels);
  params.set_effects(effects);
  params.set_mic_positions(mic_positions);
  params.set_latency_tag(latency_tag);

  *r = params;
  return r->IsValid();
}

void ParamTraits<AudioParameters>::Log(const AudioParameters& p,
                                       std::string* l) {
  l->append(base::StringPrintf("<AudioParameters>"));
}

template <>
struct ParamTraits<media::EncryptionScheme::Pattern> {
  typedef media::EncryptionScheme::Pattern param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

void ParamTraits<media::EncryptionScheme>::GetSize(base::PickleSizer* s,
                                                   const param_type& p) {
  GetParamSize(s, p.mode());
  GetParamSize(s, p.pattern());
}

void ParamTraits<media::EncryptionScheme>::Write(base::Pickle* m,
                                                 const param_type& p) {
  WriteParam(m, p.mode());
  WriteParam(m, p.pattern());
}

bool ParamTraits<media::EncryptionScheme>::Read(const base::Pickle* m,
                                                base::PickleIterator* iter,
                                                param_type* r) {
  media::EncryptionScheme::CipherMode mode;
  media::EncryptionScheme::Pattern pattern;
  if (!ReadParam(m, iter, &mode) || !ReadParam(m, iter, &pattern))
    return false;
  *r = media::EncryptionScheme(mode, pattern);
  return true;
}

void ParamTraits<media::EncryptionScheme>::Log(const param_type& p,
                                               std::string* l) {
  l->append(base::StringPrintf("<EncryptionScheme>"));
}

void ParamTraits<media::EncryptionScheme::Pattern>::GetSize(
    base::PickleSizer* s,
    const param_type& p) {
  GetParamSize(s, p.encrypt_blocks());
  GetParamSize(s, p.skip_blocks());
}

void ParamTraits<media::EncryptionScheme::Pattern>::Write(base::Pickle* m,
                                                          const param_type& p) {
  WriteParam(m, p.encrypt_blocks());
  WriteParam(m, p.skip_blocks());
}

bool ParamTraits<media::EncryptionScheme::Pattern>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  uint8_t encrypt_blocks, skip_blocks;
  if (!ReadParam(m, iter, &encrypt_blocks) || !ReadParam(m, iter, &skip_blocks))
    return false;
  *r = media::EncryptionScheme::Pattern(encrypt_blocks, skip_blocks);
  return true;
}

void ParamTraits<media::EncryptionScheme::Pattern>::Log(const param_type& p,
                                                        std::string* l) {
  l->append(base::StringPrintf("<Pattern>"));
}

}  // namespace IPC

// Generate param traits size methods.
#include "ipc/param_traits_size_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/base/ipc/media_param_traits_macros.h"
}  // namespace IPC
