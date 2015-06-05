// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_browser_client.h"

#include "chromecast/media/cma/backend/media_pipeline_device.h"
#include "content/public/browser/browser_message_filter.h"
#include "media/audio/audio_manager_factory.h"

namespace chromecast {
namespace shell {

void CastContentBrowserClient::PlatformAppendExtraCommandLineSwitches(
    base::CommandLine* command_line) {
}

std::vector<scoped_refptr<content::BrowserMessageFilter>>
CastContentBrowserClient::PlatformGetBrowserMessageFilters() {
  return std::vector<scoped_refptr<content::BrowserMessageFilter>>();
}

scoped_ptr<::media::AudioManagerFactory>
CastContentBrowserClient::PlatformCreateAudioManagerFactory() {
  // Return nullptr. The factory will not be set, and the statically linked
  // implementation of AudioManager will be used.
  return scoped_ptr<::media::AudioManagerFactory>();
}

#if !defined(OS_ANDROID)
scoped_ptr<media::MediaPipelineDevice>
CastContentBrowserClient::PlatformCreateMediaPipelineDevice(
    const media::MediaPipelineDeviceParams& params) {
  return media::CreateMediaPipelineDevice(params);
}
#endif

}  // namespace shell
}  // namespace chromecast
