// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_locale_manager_impl.h"

#include "android_webview/native/aw_contents.h"

namespace android_webview {

AwLocaleManagerImpl::AwLocaleManagerImpl() {
}

AwLocaleManagerImpl::~AwLocaleManagerImpl() {
}

std::string AwLocaleManagerImpl::GetLocale() {
  return AwContents::GetLocale();
}

}  // namespace android_webview
