// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OVERLAY_INFO_H_
#define MEDIA_BASE_OVERLAY_INFO_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/unguessable_token.h"

namespace media {

// Request information to construct an overlay.  This can be either a surface_id
// or an AndroidOverlay routing token.
using ProvideOverlayInfoCB =
    base::Callback<void(int, const base::Optional<base::UnguessableToken>&)>;
using RequestOverlayInfoCB =
    base::Callback<void(bool, const ProvideOverlayInfoCB&)>;

}  // namespace media

#endif  // MEDIA_BASE_OVERLAY_INFO_H_
