// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/web_view_handle.h"

#include "chrome/browser/profiles/profile.h"
#include "ui/views/controls/webview/webview.h"

namespace chromeos {

WebViewHandle::WebViewHandle(Profile* profile)
    : web_view_(base::MakeUnique<views::WebView>(profile)) {
  web_view_->set_owned_by_client();
}

WebViewHandle::~WebViewHandle() {}

}  // namespace chromeos
