// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace offline_pages {

struct OfflinePageItem;
class OfflinePageMetadataStore;

// Serivce for saving pages offline, storing the offline copy and metadata, and
// retrieving them upon request.
class OfflinePageModel : public KeyedService {
 public:
  explicit OfflinePageModel(scoped_ptr<OfflinePageMetadataStore> store);
  ~OfflinePageModel() override;

  // KeyedService:
  void Shutdown() override;

  // Saves the page loaded in the web contents offline.
  void SavePageOffline(const GURL& url);

  // Gets a set of all offline pages metadata.
  std::vector<OfflinePageItem> GetAllOfflinePages();

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting();

 private:
  // Persistent store for offline page metadata.
  scoped_ptr<OfflinePageMetadataStore> store_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModel);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_H_
