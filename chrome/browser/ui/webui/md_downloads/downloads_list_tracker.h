// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_

#include <set>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/download/all_download_item_notifier.h"
#include "content/public/browser/download_item.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {
class DownloadManager;
class WebUI;
}

// A class that tracks all downloads activity and keeps a sorted representation
// of the downloads as chrome://downloads wants to display them.
class DownloadsListTracker : public AllDownloadItemNotifier::Observer {
 public:
  DownloadsListTracker(content::DownloadManager* download_manager,
                       content::WebUI* web_ui);
  ~DownloadsListTracker() override;

  // Clears all downloads on the page (if it's ready).
  void CallClearAll();

  // Shows only downloads that match |search_terms|. An empty list shows all
  // downloads. Returns whether |search_terms.Equals(&search_terms_)|.
  bool SetSearchTerms(const base::ListValue& search_terms);

  // Sends all downloads and enables updates.
  void Start();

  // Stops sending updates to the page.
  void Stop();

  content::DownloadManager* GetMainNotifierManager() const;
  content::DownloadManager* GetOriginalNotifierManager() const;

  // AllDownloadItemNotifier::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;

 protected:
  // Testing constructor.
  DownloadsListTracker(content::DownloadManager* download_manager,
                       content::WebUI* web_ui,
                       base::Callback<bool(const content::DownloadItem&)>);

  // Creates a dictionary value that's sent to the page as JSON.
  virtual scoped_ptr<base::DictionaryValue> CreateDownloadItemValue(
      content::DownloadItem* item) const;

  // Exposed for testing.
  bool IsIncognito(const content::DownloadItem& item) const;

  const content::DownloadItem* GetItemForTesting(size_t index) const;

 private:
  struct StartTimeComparator {
    bool operator() (const content::DownloadItem* a,
                     const content::DownloadItem* b) const;
  };
  using SortedSet = std::set<content::DownloadItem*, StartTimeComparator>;

  // Called by both constructors to initialize common state.
  void Init();

  // Clears and re-inserts all visible items in a sorted order into
  // |sorted_visible_items_|.
  void RebuildSortedSet();

  // Whether |item| should show on the current page.
  bool ShouldShow(const content::DownloadItem& item) const;

  // Gets a page index for |position| from |sorted_visible_items_|.
  int GetIndex(const SortedSet::iterator& position) const;

  // Calls "insertItems" if |sending_updates_|.
  void CallInsertItem(const SortedSet::iterator& insert);

  // Calls "updateItem" if |sending_updates_|.
  void CallUpdateItem(const SortedSet::iterator& update);

  // Removes the item that corresponds to |remove| and sends a "removeItems"
  // message to the page if |sending_updates_|.
  void RemoveItem(const SortedSet::iterator& remove);

  AllDownloadItemNotifier main_notifier_;
  scoped_ptr<AllDownloadItemNotifier> original_notifier_;

  // The WebUI object corresponding to the page we care about.
  content::WebUI* const web_ui_;

  // Callback used to determine if an item should show on the page. Set to
  // |ShouldShow()| in default constructor, passed in while testing.
  base::Callback<bool(const content::DownloadItem&)> should_show_;

  // When this is true, all changes to downloads that affect the page are sent
  // via JavaScript.
  bool sending_updates_ = false;

  SortedSet sorted_visible_items_;

  // Current search terms.
  std::vector<base::string16> search_terms_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsListTracker);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_
