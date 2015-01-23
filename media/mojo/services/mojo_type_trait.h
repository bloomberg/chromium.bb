// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_TYPE_TRAIT_H_
#define MEDIA_MOJO_SERVICES_MOJO_TYPE_TRAIT_H_

#include "media/base/media_keys.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"

namespace media {

// A trait class to help get the corresponding mojo type for a native type.
template <typename T>
class MojoTypeTrait {};

template <>
struct MojoTypeTrait<std::string> {
  typedef mojo::String MojoType;
  static MojoType DefaultValue() { return MojoType(); }
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_TYPE_TRAIT_H_
