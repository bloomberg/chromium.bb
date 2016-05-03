// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MOJO_CHANNEL_SWITCHES_H_
#define CONTENT_PUBLIC_COMMON_MOJO_CHANNEL_SWITCHES_H_

#include "content/common/content_export.h"

namespace switches {

extern const char kEnableMojoChannel[];
CONTENT_EXPORT extern const char kMojoChannelToken[];
CONTENT_EXPORT extern const char kMojoApplicationChannelToken[];

}  // namespace switches

namespace content {

bool CONTENT_EXPORT ShouldUseMojoChannel();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MOJO_CHANNEL_SWITCHES_H_
