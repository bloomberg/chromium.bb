// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service.h"

#include "base/callback_forward.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/webrtc_event_logger.h"

WebRtcEventLogManagerKeyedService::WebRtcEventLogManagerKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(!browser_context_->IsOffTheRecord());

  content::WebRtcEventLogger* logger = content::WebRtcEventLogger::Get();
  if (logger) {
    logger->EnableForBrowserContext(browser_context_, base::OnceClosure());
    reported_ = true;
  } else {
    reported_ = false;
  }
}

void WebRtcEventLogManagerKeyedService::Shutdown() {
  content::WebRtcEventLogger* logger = content::WebRtcEventLogger::Get();
  if (logger) {
    DCHECK(reported_) << "content::WebRtcEventLogger constructed too late.";
    logger->DisableForBrowserContext(browser_context_, base::OnceClosure());
  }
}
