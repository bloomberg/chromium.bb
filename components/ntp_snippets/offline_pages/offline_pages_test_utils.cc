// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/offline_pages_test_utils.h"

#include <iterator>
#include <vector>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

using offline_pages::ClientId;
using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageItem;
using offline_pages::StubOfflinePageModel;

namespace ntp_snippets {
namespace test {

FakeOfflinePageModel::FakeOfflinePageModel() {}

FakeOfflinePageModel::~FakeOfflinePageModel() {}

void FakeOfflinePageModel::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {
  callback.Run(items_);
}

const std::vector<OfflinePageItem>& FakeOfflinePageModel::items() {
  return items_;
}

std::vector<OfflinePageItem>* FakeOfflinePageModel::mutable_items() {
  return &items_;
}

OfflinePageItem CreateDummyOfflinePageItem(int id,
                                           const std::string& name_space) {
  std::string id_string = base::IntToString(id);
  return OfflinePageItem(
      GURL("http://dummy.com/" + id_string), id,
      ClientId(name_space, base::GenerateGUID()),
      base::FilePath::FromUTF8Unsafe("some/folder/test" + id_string + ".mhtml"),
      0, base::Time::Now());
}

void CaptureDismissedSuggestions(
    std::vector<ContentSuggestion>* captured_suggestions,
    std::vector<ContentSuggestion> dismissed_suggestions) {
  std::move(dismissed_suggestions.begin(), dismissed_suggestions.end(),
            std::back_inserter(*captured_suggestions));
}

}  // namespace test
}  // namespace ntp_snippets
