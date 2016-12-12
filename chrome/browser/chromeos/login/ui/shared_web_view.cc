// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/shared_web_view.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/ui/web_view_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace chromeos {

SharedWebView::SharedWebView(Profile* profile) : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                 content::NotificationService::AllSources());
  memory_pressure_listener_ = base::MakeUnique<base::MemoryPressureListener>(
      base::Bind(&SharedWebView::OnMemoryPressure, base::Unretained(this)));
}

SharedWebView::~SharedWebView() {}

void SharedWebView::Shutdown() {
  web_view_handle_ = nullptr;
}

bool SharedWebView::Get(const GURL& url,
                        scoped_refptr<WebViewHandle>* out_handle) {
  // At the moment only one shared WebView instance is supported per profile.
  DCHECK(web_view_url_.is_empty() || web_view_url_ == url);
  web_view_url_ = url;

  // Ensure the WebView is not being reused simultaneously.
  DCHECK(!web_view_handle_ || web_view_handle_->HasOneRef());

  // Clear cached reference if it is no longer valid (ie, destroyed in task
  // manager).
  if (web_view_handle_ &&
      !web_view_handle_->web_view()
           ->GetWebContents()
           ->GetRenderViewHost()
           ->GetWidget()
           ->GetView()) {
    web_view_handle_ = nullptr;
  }

  // Create WebView if needed.
  bool reused = true;
  if (!web_view_handle_) {
    web_view_handle_ = new WebViewHandle(profile_);
    reused = false;
  }

  *out_handle = web_view_handle_;
  return reused;
}

void SharedWebView::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST, type);
  web_view_handle_ = nullptr;
}

void SharedWebView::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  switch (level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      web_view_handle_ = nullptr;
      break;
  }
}

}  // namespace chromeos
