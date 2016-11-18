// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_LOCALE_MANAGER_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_AW_LOCALE_MANAGER_IMPL_H_

#include "android_webview/browser/aw_locale_manager.h"

#include "base/macros.h"

namespace android_webview {

class AwLocaleManagerImpl : public AwLocaleManager {
 public:
  AwLocaleManagerImpl();
  ~AwLocaleManagerImpl() override;

  std::string GetLocale() override;

  std::string GetLocaleList() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwLocaleManagerImpl);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_LOCALE_MANAGER_IMPL_H_
