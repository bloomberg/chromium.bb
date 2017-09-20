// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_RECENT_TABS_RECENT_TABS_UI_ADAPTER_DELEGATE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_RECENT_TABS_RECENT_TABS_UI_ADAPTER_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/downloads/download_ui_adapter.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "url/gurl.h"

namespace offline_pages {

// Keeps track of all the tabs open for the profile, and restricts its view of
// the offline pages to only those that have an open tab.  This begins
// observation early, as soon as the first Tab is created, and even before that
// tab gets a web contents.
//
// In this UI adapter, |guid| represents the tab ID for a given page.
class RecentTabsUIAdapterDelegate : public DownloadUIAdapter::Delegate {
 public:
  explicit RecentTabsUIAdapterDelegate(OfflinePageModel* model);
  ~RecentTabsUIAdapterDelegate() override;

  static DownloadUIAdapter* GetOrCreateRecentTabsUIAdapter(
      OfflinePageModel* offline_page_model,
      RequestCoordinator* request_coordinator);
  static RecentTabsUIAdapterDelegate* FromDownloadUIAdapter(
      DownloadUIAdapter* adapter);
  // This extracts the tab ID out of the |id| field of the client ID using a
  // string conversion.  Crashes if conversion fails.
  static int TabIdFromClientId(const ClientId& client_id);

  // This override returns true if the client ID is shown on the NTP as a recent
  // tab.  This is determined by policy.
  bool IsVisibleInUI(const ClientId& client_id) override;
  // This returns true if there does not exist a tab ID for the given client ID.
  bool IsTemporarilyHiddenInUI(const ClientId& client_id) override;
  // Sets our reference to the UI adapter so we can notify it of visibility
  // changes.
  void SetUIAdapter(DownloadUIAdapter* ui_adapter) override;
  void OpenItem(const OfflineItem& item, int64_t offline_id) override {}

  // Register/UnregisterTab add and remove tab IDs from the list.  These
  // functions can be called before a page actually exists with the given tab
  // ID.
  // Note that these change temporary visibility and therefore |set_ui_adapter|
  // must be called before either of these functions.
  void RegisterTab(int tab_id);
  void UnregisterTab(int tab_id);

 private:
  // Constructs the client ID assuming that we use the Last N namespace.
  //
  // TODO(dewittj): Make this more resilient to adding more namespaces.
  static ClientId ClientIdFromTabId(int tab_id);

  // Checks a client ID for proper namespace and ID format to be shown in the
  // Downloads Home UI.
  bool IsRecentTab(const ClientId& page);

  // Always valid, this class is owned by DownloadUIAdapter, which is owned by
  // Offline Page Model.
  OfflinePageModel* model_;

  // This is set by the UI adapter itself.
  DownloadUIAdapter* ui_adapter_ = nullptr;

  // The tabs we've seen.
  std::unordered_set<int> active_tabs_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabsUIAdapterDelegate);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_RECENT_TABS_RECENT_TABS_UI_ADAPTER_DELEGATE_H_
