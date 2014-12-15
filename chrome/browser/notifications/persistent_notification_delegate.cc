// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_delegate.h"

#include "base/bind.h"
#include "base/guid.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "content/public/common/persistent_notification_status.h"

namespace {

// Persistent notifications fired through the delegate do not care about the
// lifetime of the Service Worker responsible for executing the event.
void OnEventDispatchComplete(content::PersistentNotificationStatus status) {}

}  // namespace

PersistentNotificationDelegate::PersistentNotificationDelegate(
    content::BrowserContext* browser_context,
    int64 service_worker_registration_id,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data)
    : browser_context_(browser_context),
      service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      notification_data_(notification_data),
      id_(base::GenerateGUID()) {}

PersistentNotificationDelegate::~PersistentNotificationDelegate() {}

void PersistentNotificationDelegate::Display() {}

void PersistentNotificationDelegate::Close(bool by_user) {}

void PersistentNotificationDelegate::Click() {
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      browser_context_,
      service_worker_registration_id_,
      id_,
      origin_,
      notification_data_,
      base::Bind(&OnEventDispatchComplete));
}

std::string PersistentNotificationDelegate::id() const {
  return id_;
}
