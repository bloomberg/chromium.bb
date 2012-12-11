// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notify_channel_ui_android.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

AppNotifyChannelUIAndroid::AppNotifyChannelUIAndroid() {}

AppNotifyChannelUIAndroid::~AppNotifyChannelUIAndroid() {
}

// static
AppNotifyChannelUI* AppNotifyChannelUI::Create(Profile* profile,
    content::WebContents* web_contents,
    const std::string& app_name,
    AppNotifyChannelUI::UIType ui_type) {
  return new AppNotifyChannelUIAndroid();
}

void AppNotifyChannelUIAndroid::PromptSyncSetup(
    AppNotifyChannelUI::Delegate* delegate) {
  NOTIMPLEMENTED();
}

}  // namespace extensions
