// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/recent_tabs/recent_tabs_ui_adapter_delegate.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_model.h"

namespace offline_pages {

namespace {
const char kRecentTabsUIAdapterKey[] = "recent-tabs-download-ui-adapter";
}  // namespace

RecentTabsUIAdapterDelegate::RecentTabsUIAdapterDelegate(
    OfflinePageModel* model)
    : model_(model) {}

RecentTabsUIAdapterDelegate::~RecentTabsUIAdapterDelegate() = default;

offline_pages::DownloadUIAdapter*
RecentTabsUIAdapterDelegate::GetOrCreateRecentTabsUIAdapter(
    offline_pages::OfflinePageModel* offline_page_model,
    offline_pages::RequestCoordinator* request_coordinator) {
  offline_pages::DownloadUIAdapter* recent_tabs_ui_adapter =
      static_cast<DownloadUIAdapter*>(
          offline_page_model->GetUserData(kRecentTabsUIAdapterKey));

  if (!recent_tabs_ui_adapter) {
    auto delegate =
        base::MakeUnique<RecentTabsUIAdapterDelegate>(offline_page_model);
    recent_tabs_ui_adapter = new DownloadUIAdapter(
        nullptr, offline_page_model, request_coordinator, std::move(delegate));
    offline_page_model->SetUserData(kRecentTabsUIAdapterKey,
                                    base::WrapUnique(recent_tabs_ui_adapter));
  }

  return recent_tabs_ui_adapter;
}

// static
RecentTabsUIAdapterDelegate* RecentTabsUIAdapterDelegate::FromDownloadUIAdapter(
    DownloadUIAdapter* ui_adapter) {
  return static_cast<RecentTabsUIAdapterDelegate*>(ui_adapter->delegate());
}

// static
int RecentTabsUIAdapterDelegate::TabIdFromClientId(const ClientId& client_id) {
  int tab_id = 0;
  bool convert_result = base::StringToInt(client_id.id, &tab_id);
  DCHECK(convert_result);
  return tab_id;
}

bool RecentTabsUIAdapterDelegate::IsVisibleInUI(const ClientId& client_id) {
  return IsRecentTab(client_id);
}

bool RecentTabsUIAdapterDelegate::IsTemporarilyHiddenInUI(
    const ClientId& client_id) {
  return active_tabs_.count(TabIdFromClientId(client_id)) == 0;
}

void RecentTabsUIAdapterDelegate::SetUIAdapter(DownloadUIAdapter* adapter) {
  ui_adapter_ = adapter;
}

void RecentTabsUIAdapterDelegate::RegisterTab(int tab_id) {
  DCHECK(ui_adapter_);
  if (active_tabs_.count(tab_id) > 0)
    return;
  active_tabs_.insert(tab_id);
  ui_adapter_->TemporaryHiddenStatusChanged(ClientIdFromTabId(tab_id));
}

void RecentTabsUIAdapterDelegate::UnregisterTab(int tab_id) {
  DCHECK(ui_adapter_);
  if (active_tabs_.count(tab_id) == 0)
    return;
  active_tabs_.erase(tab_id);
  ui_adapter_->TemporaryHiddenStatusChanged(ClientIdFromTabId(tab_id));
}

// static
ClientId RecentTabsUIAdapterDelegate::ClientIdFromTabId(int tab_id) {
  return ClientId(kLastNNamespace, base::IntToString(tab_id));
}

bool RecentTabsUIAdapterDelegate::IsRecentTab(const ClientId& page) {
  return model_->GetPolicyController()->IsShownAsRecentlyVisitedSite(
      page.name_space);
}

}  // namespace offline_pages
