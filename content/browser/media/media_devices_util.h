// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"

namespace content {

// Returns the ID of the user-default device ID via |callback|.
// If no such device ID can be found, |callback| receives an empty string.
void CONTENT_EXPORT GetDefaultMediaDeviceID(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    const base::Callback<void(const std::string&)>& callback);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
