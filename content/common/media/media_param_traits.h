// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_PARAM_TRAITS_H_
#define CONTENT_COMMON_MEDIA_MEDIA_PARAM_TRAITS_H_

#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"

namespace media {
class AudioParameters;
class VideoCaptureParams;
}

namespace IPC {

template <>
struct ParamTraits<media::AudioParameters> {
  typedef media::AudioParameters param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<media::VideoCaptureParams> {
  typedef media::VideoCaptureParams param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

} // namespace IPC

#endif  // CONTENT_COMMON_MEDIA_MEDIA_PARAM_TRAITS_H_
