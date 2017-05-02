// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ROUTING_TOKEN_CALLBACK_H_
#define MEDIA_BASE_ROUTING_TOKEN_CALLBACK_H_

#include "base/callback.h"
#include "base/unguessable_token.h"

namespace media {

// Handy callback type to provide a routing token.
using RoutingTokenCallback =
    base::Callback<void(const base::UnguessableToken&)>;

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_ROUTING_TOKEN_CALLBACK_H_
