// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"

namespace notifier {
ChromeNotifierDelegate::ChromeNotifierDelegate(const std::string& id,
                                               ChromeNotifierService* notifier)
    : id_(id), chrome_notifier_(notifier) {}

ChromeNotifierDelegate::~ChromeNotifierDelegate() {}

std::string ChromeNotifierDelegate::id() const {
  return id_;
}

content::RenderViewHost* ChromeNotifierDelegate::GetRenderViewHost() const {
    return NULL;
}

void ChromeNotifierDelegate::Close(bool by_user) {
  chrome_notifier_->MarkNotificationAsDismissed(id_);
}

}  // namespace notifier
