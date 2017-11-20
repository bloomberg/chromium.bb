// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace history {
class HistoryService;
}  // namespace history

class InMemoryURLIndex;

// Fully operational AutocompleteProviderClient for usage in tests.
class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient();
  ~FakeAutocompleteProviderClient() override;

  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  const SearchTermsData& GetSearchTermsData() const override;
  history::HistoryService* GetHistoryService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  InMemoryURLIndex* GetInMemoryURLIndex() override;

  void set_in_memory_url_index(std::unique_ptr<InMemoryURLIndex> index) {
    in_memory_url_index_ = std::move(index);
  }

  bool IsTabOpenWithURL(const GURL& url) override;
  void set_is_tab_open_with_url(bool is_open) {
    is_tab_open_with_url_ = is_open;
  }

 private:
  base::ScopedTempDir history_dir_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  TestSchemeClassifier scheme_classifier_;
  SearchTermsData search_terms_data_;
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index_;
  std::unique_ptr<history::HistoryService> history_service_;
  bool is_tab_open_with_url_;

  DISALLOW_COPY_AND_ASSIGN(FakeAutocompleteProviderClient);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_
