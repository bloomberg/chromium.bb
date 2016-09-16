// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_service.h"

#include "components/previews/core/previews_ui_service.h"
#include "content/public/browser/browser_thread.h"

PreviewsService::PreviewsService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PreviewsService::~PreviewsService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PreviewsService::set_previews_ui_service(
    std::unique_ptr<previews::PreviewsUIService> previews_ui_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  previews_ui_service_ = std::move(previews_ui_service);
}
