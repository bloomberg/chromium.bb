// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_ITEM_GENERATOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_ITEM_GENERATOR_H_

#include <string>

#include "components/offline_pages/core/offline_page_item.h"

class GURL;

namespace offline_pages {

// Class for generating offline page items for testing.
class OfflinePageItemGenerator {
 public:
  OfflinePageItemGenerator();
  ~OfflinePageItemGenerator();

  OfflinePageItem CreateItem();

  void SetNamespace(const std::string& name_space);
  void SetRequestOrigin(const std::string& request_origin);
  void SetUrl(const GURL& url);
  void SetOriginalUrl(const GURL& url);

 private:
  std::string namespace_;
  std::string request_origin_;
  GURL url_;
  GURL original_url_;
};
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_ITEM_GENERATOR_H_
