// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/content_index_provider.h"

class Profile;

class ContentIndexProviderImpl : public KeyedService,
                                 public content::ContentIndexProvider {
 public:
  explicit ContentIndexProviderImpl(Profile* profile);

  // KeyedService implementation.
  void Shutdown() override;

  // ContentIndexProvider implementation.
  void OnContentAdded(
      content::ContentIndexEntry entry,
      base::WeakPtr<content::ContentIndexProvider::Client> client) override;
  void OnContentDeleted(int64_t service_worker_registration_id,
                        const std::string& description_id) override;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexProviderImpl);
};

#endif  // CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_IMPL_H_
