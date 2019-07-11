// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_provider_impl.h"

#include "content/public/browser/browser_thread.h"

ContentIndexProviderImpl::ContentIndexProviderImpl(Profile* profile) {}

void ContentIndexProviderImpl::Shutdown() {}

void ContentIndexProviderImpl::OnContentAdded(
    content::ContentIndexEntry entry,
    base::WeakPtr<content::ContentIndexProvider::Client> client) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ContentIndexProviderImpl::OnContentDeleted(
    int64_t service_worker_registration_id,
    const std::string& description_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}
