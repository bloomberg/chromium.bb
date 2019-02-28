// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_FEATURES_H_
#define MOJO_PUBLIC_CPP_PLATFORM_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace mojo {
namespace features {

#if defined(OS_MACOSX) && !defined(OS_IOS)
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
extern const base::Feature kMojoChannelMac;
#endif

}  // namespace features
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_FEATURES_H_
