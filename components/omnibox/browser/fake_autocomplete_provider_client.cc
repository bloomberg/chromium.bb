// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fake_autocomplete_provider_client.h"

#include "base/memory/ptr_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/in_memory_url_index_test_util.h"

namespace {

// Arbitrary number > 1 so that tasks don't run sequentially.
constexpr int kWorkerThreadCount = 3;

}  // namespace

FakeAutocompleteProviderClient::FakeAutocompleteProviderClient()
    : pool_owner_(kWorkerThreadCount, "Background Pool") {
  bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

  CHECK(history_dir_.CreateUniqueTempDir());
  history_service_ =
      history::CreateHistoryService(history_dir_.GetPath(), true);

  in_memory_url_index_.reset(new InMemoryURLIndex(
      bookmark_model_.get(), history_service_.get(), nullptr,
      pool_owner_.pool().get(), history_dir_.GetPath(), SchemeSet()));
  in_memory_url_index_->Init();
}

FakeAutocompleteProviderClient::~FakeAutocompleteProviderClient() = default;

const AutocompleteSchemeClassifier&
FakeAutocompleteProviderClient::GetSchemeClassifier() const {
  return scheme_classifier_;
}

const SearchTermsData& FakeAutocompleteProviderClient::GetSearchTermsData()
    const {
  return search_terms_data_;
}

history::HistoryService* FakeAutocompleteProviderClient::GetHistoryService() {
  return history_service_.get();
}

bookmarks::BookmarkModel* FakeAutocompleteProviderClient::GetBookmarkModel() {
  return bookmark_model_.get();
}

InMemoryURLIndex* FakeAutocompleteProviderClient::GetInMemoryURLIndex() {
  return in_memory_url_index_.get();
}
