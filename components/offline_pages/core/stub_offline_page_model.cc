// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/stub_offline_page_model.h"

#include "base/files/file_path.h"

namespace offline_pages {

StubOfflinePageModel::StubOfflinePageModel()
    : archive_directory_(base::FilePath(FILE_PATH_LITERAL("/archive_dir/"))) {}
StubOfflinePageModel::~StubOfflinePageModel() {}

void StubOfflinePageModel::AddObserver(Observer* observer) {}
void StubOfflinePageModel::RemoveObserver(Observer* observer) {}
void StubOfflinePageModel::SavePage(
    const SavePageParams& save_page_params,
    std::unique_ptr<OfflinePageArchiver> archiver,
    const SavePageCallback& callback) {}
void StubOfflinePageModel::AddPage(const OfflinePageItem& page,
                                   const AddPageCallback& callback) {}
void StubOfflinePageModel::MarkPageAccessed(int64_t offline_id) {}
void StubOfflinePageModel::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {}
void StubOfflinePageModel::DeletePagesByClientIds(
    const std::vector<ClientId>& client_ids,
    const DeletePageCallback& callback) {}
void StubOfflinePageModel::GetPagesByClientIds(
    const std::vector<ClientId>& client_ids,
    const MultipleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::DeleteCachedPagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {}
void StubOfflinePageModel::CheckPagesExistOffline(
    const std::set<GURL>& urls,
    const CheckPagesExistOfflineCallback& callback) {}
void StubOfflinePageModel::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::GetOfflineIdsForClientId(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) {}
void StubOfflinePageModel::GetPageByOfflineId(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::GetPagesByURL(
    const GURL& url,
    URLSearchMode url_search_mode,
    const MultipleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::GetPagesByRequestOrigin(
    const std::string& origin,
    const MultipleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::GetPagesRemovedOnCacheReset(
    const MultipleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::GetPagesByNamespace(
    const std::string& name_space,
    const MultipleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::GetPagesSupportedByDownloads(
    const MultipleOfflinePageItemCallback& callback) {}
const base::FilePath& StubOfflinePageModel::GetArchiveDirectory(
    const std::string& name_space) const {
  return archive_directory_;
}

ClientPolicyController* StubOfflinePageModel::GetPolicyController() {
  return &policy_controller_;
}
bool StubOfflinePageModel::is_loaded() const {
  return true;
}
OfflineEventLogger* StubOfflinePageModel::GetLogger() {
  return nullptr;
}
}  // namespace offline_pages
