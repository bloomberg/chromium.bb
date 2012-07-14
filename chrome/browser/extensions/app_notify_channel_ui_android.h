// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/app_notify_channel_ui.h"

class AppNotifyChannelUIAndroid : public AppNotifyChannelUI {
 public:
  AppNotifyChannelUIAndroid();
  virtual ~AppNotifyChannelUIAndroid();

  // AppNotifyChannelUI.
  virtual void PromptSyncSetup(AppNotifyChannelUI::Delegate* delegate) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppNotifyChannelUIAndroid);
};

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_UI_ANDROID_H_
