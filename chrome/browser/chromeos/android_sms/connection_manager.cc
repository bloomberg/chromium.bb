// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/connection_manager.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

namespace android_sms {

ConnectionManager::ConnectionManager(
    content::ServiceWorkerContext* service_worker_context,
    std::unique_ptr<ConnectionEstablisher> connection_establisher)
    : service_worker_context_(service_worker_context),
      connection_establisher_(std::move(connection_establisher)) {
  service_worker_context_->AddObserver(this);
  connection_establisher_->EstablishConnection(service_worker_context_);
}

ConnectionManager::~ConnectionManager() {
  service_worker_context_->RemoveObserver(this);
}

void ConnectionManager::OnVersionActivated(int64_t version_id,
                                           const GURL& scope) {
  if (!scope.EqualsIgnoringRef(GetAndroidMessagesURL()))
    return;

  prev_active_version_id_ = active_version_id_;
  active_version_id_ = version_id;
  connection_establisher_->EstablishConnection(service_worker_context_);
}

void ConnectionManager::OnVersionRedundant(int64_t version_id,
                                           const GURL& scope) {
  if (!scope.EqualsIgnoringRef(GetAndroidMessagesURL()))
    return;

  if (active_version_id_ != version_id)
    return;

  // If the active version is marked redundant then it cannot handle messages
  // anymore, so stop tracking it.
  prev_active_version_id_ = active_version_id_;
  active_version_id_.reset();
}

void ConnectionManager::OnNoControllees(int64_t version_id, const GURL& scope) {
  if (!scope.EqualsIgnoringRef(GetAndroidMessagesURL()))
    return;

  // Set active_version_id_ in case we missed version activated.
  // This is unlikely but protects against a case where a Android Messages for
  // Web page may have opened before the ConnectionManager is created.
  if (!active_version_id_ && prev_active_version_id_ != version_id)
    active_version_id_ = version_id;

  if (active_version_id_ != version_id)
    return;

  connection_establisher_->EstablishConnection(service_worker_context_);
}

}  // namespace android_sms

}  // namespace chromeos
