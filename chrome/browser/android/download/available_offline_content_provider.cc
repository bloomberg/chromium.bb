// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/available_offline_content_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/base/l10n/time_format.h"

namespace android {
using offline_items_collection::OfflineItem;

namespace {
// Ordered in order of decreasing priority.
enum class ContentPriority {
  kSuggestedUnopenedPage,
  kVideo,
  kAudio,
  kDownloadedPage,
  kDoNotShow
};

bool ItemHasBeenOpened(const OfflineItem& item) {
  // Typically, items are initialized with an accessed_time equal to the
  // creation_time.
  return !item.last_accessed_time.is_null() &&
         item.last_accessed_time > item.creation_time;
}

ContentPriority ContentTypePriority(const OfflineItem& item) {
  switch (item.filter) {
    case offline_items_collection::FILTER_PAGE:  // fallthrough
    case offline_items_collection::FILTER_DOCUMENT:
      if (item.is_suggested)
        return ItemHasBeenOpened(item)
                   ? ContentPriority::kDoNotShow
                   : ContentPriority::kSuggestedUnopenedPage;
      return ContentPriority::kDownloadedPage;
      break;
    case offline_items_collection::FILTER_VIDEO:
      return ContentPriority::kVideo;
    case offline_items_collection::FILTER_AUDIO:
      return ContentPriority::kAudio;
    default:
      break;
  }
  return ContentPriority::kDoNotShow;
}

bool CompareItemsByUsefulness(const OfflineItem& a, const OfflineItem& b) {
  ContentPriority a_priority = ContentTypePriority(a);
  ContentPriority b_priority = ContentTypePriority(b);
  if (a_priority != b_priority)
    return a_priority < b_priority;
  // Break a tie by creation_time: more recent first.
  if (a.creation_time != b.creation_time)
    return a.creation_time > b.creation_time;
  // Make sure only one ordering is possible.
  return a.id < b.id;
}

// Picks the best available offline content items, and passes them to callback.
void ListFinalize(AvailableOfflineContentProvider::ListCallback callback,
                  const std::vector<OfflineItem>& all_items) {
  // Save the best 3 or fewer times to |selected|.
  const int kMaxItemsToReturn = 3;
  std::vector<OfflineItem> selected(kMaxItemsToReturn);
  const auto end = std::partial_sort_copy(all_items.begin(), all_items.end(),
                                          selected.begin(), selected.end(),
                                          CompareItemsByUsefulness);
  selected.resize(end - selected.begin());

  // Translate OfflineItem to AvailableOfflineContentPtr, and filter out
  // items that should not be shown.
  std::vector<chrome::mojom::AvailableOfflineContentPtr> result;
  for (const OfflineItem& item : selected) {
    if (ContentTypePriority(item) >= ContentPriority::kDoNotShow)
      break;
    result.push_back(chrome::mojom::AvailableOfflineContent::New(
        item.id.id, item.id.name_space, item.title, item.description,
        base::UTF16ToASCII(ui::TimeFormat::Simple(
            ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
            base::Time::Now() - item.creation_time)),
        "",     // TODO(crbug.com/852872): Add attribution
        GURL()  // TODO(crbug.com/852872): Add thumbnail data URL
        ));
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace

AvailableOfflineContentProvider::AvailableOfflineContentProvider(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AvailableOfflineContentProvider::~AvailableOfflineContentProvider() = default;

void AvailableOfflineContentProvider::List(ListCallback callback) {
  if (!base::FeatureList::IsEnabled(chrome::android::kNewNetErrorPageUI)) {
    std::move(callback).Run({});
    return;
  }
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForBrowserContext(browser_context_);
  aggregator->GetAllItems(base::BindOnce(ListFinalize, std::move(callback)));
}

void AvailableOfflineContentProvider::Create(
    content::BrowserContext* browser_context,
    chrome::mojom::AvailableOfflineContentProviderRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<AvailableOfflineContentProvider>(browser_context),
      std::move(request));
}

}  // namespace android
