// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/history.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/history_data.h"
#include "chrome/browser/ui/app_list/search/history_data_store.h"
#include "chrome/browser/ui/app_list/search/tokenized_string.h"
#include "content/public/browser/browser_context.h"

namespace app_list {

namespace {

// Normalize the given string by joining all its tokens with a space.
std::string NormalizeString(const std::string& utf8) {
  TokenizedString tokenized(base::UTF8ToUTF16(utf8));
  return base::UTF16ToUTF8(JoinString(tokenized.tokens(), ' '));
}

}  // namespace

History::History(content::BrowserContext* context)
    : browser_context_(context),
      data_loaded_(false) {
  const char kStoreDataFileName[] = "App Launcher Search";
  const size_t kMaxQueryEntries = 1000;
  const size_t kMaxSecondaryQueries = 5;

  const base::FilePath data_file =
      browser_context_->GetPath().AppendASCII(kStoreDataFileName);
  store_ = new HistoryDataStore(data_file);
  data_.reset(
      new HistoryData(store_.get(), kMaxQueryEntries, kMaxSecondaryQueries));
  data_->AddObserver(this);
}

History::~History() {
  data_->RemoveObserver(this);
}

bool History::IsReady() const {
  return data_loaded_;
}

void History::AddLaunchEvent(const std::string& query,
                             const std::string& result_id) {
  DCHECK(IsReady());
  data_->Add(NormalizeString(query), result_id);
}

scoped_ptr<KnownResults> History::GetKnownResults(
    const std::string& query) const {
  DCHECK(IsReady());
  return data_->GetKnownResults(NormalizeString(query)).Pass();
}

void History::OnHistoryDataLoadedFromStore() {
  data_loaded_ = true;
}

}  // namespace app_list
