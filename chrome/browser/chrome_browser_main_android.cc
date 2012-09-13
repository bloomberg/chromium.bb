// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_android.h"

#include "chrome/common/chrome_switches.h"
#include "content/public/common/main_function_params.h"
#include "net/android/network_change_notifier_factory.h"
#include "net/base/network_change_notifier.h"

ChromeBrowserMainPartsAndroid::ChromeBrowserMainPartsAndroid(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainParts(parameters) {
}

ChromeBrowserMainPartsAndroid::~ChromeBrowserMainPartsAndroid() {
}

void ChromeBrowserMainPartsAndroid::PreEarlyInitialization() {
  net::NetworkChangeNotifier::SetFactory(
      new net::android::NetworkChangeNotifierFactory());

  // Chrome on Android does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  DCHECK(!main_message_loop_.get());
  main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));
  MessageLoopForUI::current()->Start();

  ChromeBrowserMainParts::PreEarlyInitialization();
}

void ChromeBrowserMainPartsAndroid::ShowMissingLocaleMessageBox() {
  NOTREACHED();
}

void RecordBreakpadStatusUMA(MetricsService* metrics) {
  // TODO: crbug.com/139023
  NOTIMPLEMENTED();
}

void WarnAboutMinimumSystemRequirements() {
}
