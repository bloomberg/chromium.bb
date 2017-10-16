// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"
#include "url/origin.h"

namespace content {

// Returns the ID of the user-default device ID via |callback|.
// If no such device ID can be found, |callback| receives an empty string.
void CONTENT_EXPORT GetDefaultMediaDeviceID(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    const base::Callback<void(const std::string&)>& callback);

// Returns the current media device ID salt and security origin for the given
// |render_process_id| and |render_frame_id|. These values are used to produce
// unique media-device IDs for each origin and renderer process. These values
// should not be cached since the user can explicitly change them at any time.
// This function must run on the UI thread.
std::pair<std::string, url::Origin> GetMediaDeviceSaltAndOrigin(
    int render_process_id,
    int render_frame_id);

// Type definition to make it easier to use mock alternatives to
// GetMediaDeviceSaltAndOrigin.
using MediaDeviceSaltAndOriginCallback =
    base::RepeatingCallback<std::pair<std::string, url::Origin>(int, int)>;

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
