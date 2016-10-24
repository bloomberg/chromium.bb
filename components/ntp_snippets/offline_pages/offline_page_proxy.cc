// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/offline_page_proxy.h"

#include "base/bind.h"

using offline_pages::MultipleOfflinePageItemResult;
using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageModel;

namespace ntp_snippets {

OfflinePageProxy::OfflinePageProxy(OfflinePageModel* offline_page_model)
    : offline_page_model_(offline_page_model), weak_ptr_factory_(this) {
  offline_page_model_->AddObserver(this);
}

void OfflinePageProxy::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {
  offline_page_model_->GetAllPages(callback);
}

void OfflinePageProxy::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OfflinePageProxy::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

OfflinePageProxy::~OfflinePageProxy() {
  offline_page_model_->RemoveObserver(this);
}

void OfflinePageProxy::OfflinePageModelLoaded(OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
}

void OfflinePageProxy::OfflinePageModelChanged(OfflinePageModel* model) {
  DCHECK_EQ(offline_page_model_, model);
  FetchOfflinePagesAndNotify();
}

void OfflinePageProxy::OfflinePageDeleted(
    int64_t offline_id,
    const offline_pages::ClientId& client_id) {
  for (Observer& observer : observers_)
    observer.OfflinePageDeleted(offline_id, client_id);
}

void OfflinePageProxy::FetchOfflinePagesAndNotify() {
  offline_page_model_->GetAllPages(base::Bind(
      &OfflinePageProxy::OnOfflinePagesLoaded, weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageProxy::OnOfflinePagesLoaded(
    const MultipleOfflinePageItemResult& result) {
  for (Observer& observer : observers_)
    observer.OfflinePageModelChanged(result);
}

}  // namespace ntp_snippets
