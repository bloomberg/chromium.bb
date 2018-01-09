// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_utils.h"

#include <string>

#include "base/logging.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_item.h"

namespace offline_pages {

namespace model_utils {

OfflinePagesNamespaceEnumeration ToNamespaceEnum(
    const std::string& name_space) {
  if (name_space == kDefaultNamespace)
    return OfflinePagesNamespaceEnumeration::DEFAULT;
  else if (name_space == kBookmarkNamespace)
    return OfflinePagesNamespaceEnumeration::BOOKMARK;
  else if (name_space == kLastNNamespace)
    return OfflinePagesNamespaceEnumeration::LAST_N;
  else if (name_space == kAsyncNamespace)
    return OfflinePagesNamespaceEnumeration::ASYNC_LOADING;
  else if (name_space == kCCTNamespace)
    return OfflinePagesNamespaceEnumeration::CUSTOM_TABS;
  else if (name_space == kDownloadNamespace)
    return OfflinePagesNamespaceEnumeration::DOWNLOAD;
  else if (name_space == kNTPSuggestionsNamespace)
    return OfflinePagesNamespaceEnumeration::NTP_SUGGESTION;
  else if (name_space == kSuggestedArticlesNamespace)
    return OfflinePagesNamespaceEnumeration::SUGGESTED_ARTICLES;
  else if (name_space == kBrowserActionsNamespace)
    return OfflinePagesNamespaceEnumeration::BROWSER_ACTIONS;

  NOTREACHED();
  return OfflinePagesNamespaceEnumeration::DEFAULT;
}

std::string AddHistogramSuffix(const std::string& name_space,
                               const char* histogram_name) {
  if (name_space.empty()) {
    NOTREACHED();
    return histogram_name;
  }
  std::string adjusted_histogram_name(histogram_name);
  adjusted_histogram_name += ".";
  adjusted_histogram_name += name_space;
  return adjusted_histogram_name;
}

}  // namespace model_utils

}  // namespace offline_pages
