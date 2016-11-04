// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/sequenced_worker_pool_owner.h"
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

 private:
  base::SequencedWorkerPoolOwner pool_owner_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  TestSchemeClassifier scheme_classifier_;
  SearchTermsData search_terms_data_;
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index_;
  std::unique_ptr<history::HistoryService> history_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeAutocompleteProviderClient);
};
