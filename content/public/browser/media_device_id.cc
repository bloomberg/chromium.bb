// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/browser/media_device_id.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"

namespace content {

std::string GetHMACForMediaDeviceID(const ResourceContext::SaltCallback& sc,
                                    const GURL& security_origin,
                                    const std::string& raw_unique_id) {
  return MediaStreamManager::GetHMACForMediaDeviceID(sc, security_origin,
                                                     raw_unique_id);
}

bool DoesMediaDeviceIDMatchHMAC(const ResourceContext::SaltCallback& sc,
                                const GURL& security_origin,
                                const std::string& device_guid,
                                const std::string& raw_unique_id) {
  return MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
      sc, security_origin, device_guid, raw_unique_id);
}

bool GetMediaDeviceIDForHMAC(MediaStreamType stream_type,
                             const ResourceContext::SaltCallback& rc,
                             const GURL& security_origin,
                             const std::string& source_id,
                             std::string* device_id) {
  content::MediaStreamManager* manager =
      content::BrowserMainLoop::GetInstance()->media_stream_manager();

  return manager->TranslateSourceIdToDeviceId(
      content::MEDIA_DEVICE_VIDEO_CAPTURE,
      rc,
      security_origin,
      source_id,
      device_id);
}

}  // namespace content
