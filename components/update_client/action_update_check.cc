// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_update_check.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "components/update_client/action_update.h"
#include "components/update_client/configurator.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

using std::string;
using std::vector;

namespace update_client {

namespace {

// Returns true if the |proposed| version is newer than |current| version.
bool IsVersionNewer(const base::Version& current, const std::string& proposed) {
  base::Version proposed_ver(proposed);
  return proposed_ver.IsValid() && current.CompareTo(proposed_ver) < 0;
}

}  // namespace

ActionUpdateCheck::ActionUpdateCheck(
    std::unique_ptr<UpdateChecker> update_checker,
    const base::Version& browser_version,
    const std::string& extra_request_parameters)
    : update_checker_(std::move(update_checker)),
      browser_version_(browser_version),
      extra_request_parameters_(extra_request_parameters) {}

ActionUpdateCheck::~ActionUpdateCheck() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ActionUpdateCheck::Run(UpdateContext* update_context, Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ActionImpl::Run(update_context, callback);

  // Calls out to get the corresponding CrxComponent data for the CRXs in this
  // update context.
  vector<CrxComponent> crx_components;
  update_context_->crx_data_callback.Run(update_context_->ids, &crx_components);

  for (size_t i = 0; i != crx_components.size(); ++i) {
    std::unique_ptr<CrxUpdateItem> item = base::MakeUnique<CrxUpdateItem>();
    const CrxComponent& crx_component = crx_components[i];

    item->id = GetCrxComponentID(crx_component);
    item->component = crx_component;
    item->last_check = base::TimeTicks::Now();
    item->crx_urls.clear();
    item->crx_diffurls.clear();
    item->previous_version = crx_component.version;
    item->next_version = base::Version();
    item->previous_fp = crx_component.fingerprint;
    item->next_fp.clear();
    item->on_demand = update_context->is_foreground;
    item->diff_update_failed = false;
    item->error_category = 0;
    item->error_code = 0;
    item->extra_code1 = 0;
    item->diff_error_category = 0;
    item->diff_error_code = 0;
    item->diff_extra_code1 = 0;
    item->download_metrics.clear();

    CrxUpdateItem* item_ptr = item.get();
    update_context_->update_items[item_ptr->id] = std::move(item);

    ChangeItemState(item_ptr, CrxUpdateItem::State::kChecking);
  }

  update_checker_->CheckForUpdates(
      update_context_->update_items, extra_request_parameters_,
      update_context_->enabled_component_updates,
      base::Bind(&ActionUpdateCheck::UpdateCheckComplete,
                 base::Unretained(this)));
}

void ActionUpdateCheck::UpdateCheckComplete(
    int error,
    const UpdateResponse::Results& results,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  update_context_->retry_after_sec = retry_after_sec;

  if (!error)
    OnUpdateCheckSucceeded(results);
  else
    OnUpdateCheckFailed(error);
}

void ActionUpdateCheck::OnUpdateCheckSucceeded(
    const UpdateResponse::Results& results) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Update check succeeded.";

  for (const auto& result : results.list) {
    CrxUpdateItem* crx = FindUpdateItemById(result.extension_id);
    if (!crx) {
      VLOG(1) << "Component not found " << result.extension_id;
      continue;
    }

    DCHECK_EQ(CrxUpdateItem::State::kChecking, crx->state);

    if (result.status == "ok")
      HandleUpdateCheckOK(result, crx);
    else if (result.status == "noupdate")
      HandleUpdateCheckNoupdate(result, crx);
    else
      HandleUpdateCheckError(result, crx);
  }

  // All components that are not included in the update response are
  // considered up to date.
  ChangeAllItemsState(CrxUpdateItem::State::kChecking,
                      CrxUpdateItem::State::kUpToDate);

  if (update_context_->queue.empty()) {
    VLOG(1) << "Update check completed but no action is needed.";
    UpdateComplete(Error::NONE);
    return;
  }

  // Starts the execution flow of updating the CRXs in this context.
  UpdateCrx();
}

void ActionUpdateCheck::HandleUpdateCheckOK(
    const UpdateResponse::Result& result,
    CrxUpdateItem* crx) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& manifest = result.manifest;

  if (manifest.version.empty()) {
    // It can't update without a manifest version.
    VLOG(1) << "No manifest version available for CRX: " << crx->id;
    ChangeItemState(crx, CrxUpdateItem::State::kNoUpdate);
    return;
  }

  if (!IsVersionNewer(crx->component.version, manifest.version)) {
    // The CRX is up to date.
    VLOG(1) << "Component already up to date: " << crx->id;
    ChangeItemState(crx, CrxUpdateItem::State::kUpToDate);
    return;
  }

  if (!manifest.browser_min_version.empty()) {
    if (IsVersionNewer(browser_version_, manifest.browser_min_version)) {
      // The CRX is not compatible with this Chrome version.
      VLOG(1) << "Ignoring incompatible CRX: " << crx->id;
      ChangeItemState(crx, CrxUpdateItem::State::kNoUpdate);
      return;
    }
  }

  if (manifest.packages.size() != 1) {
    // Assume one and only one package per CRX.
    VLOG(1) << "Ignoring multiple packages for CRX: " << crx->id;
    ChangeItemState(crx, CrxUpdateItem::State::kNoUpdate);
    return;
  }

  // Parse the members of the result and queue an upgrade for this CRX.
  VLOG(1) << "Update found for CRX: " << crx->id;

  crx->next_version = base::Version(manifest.version);
  const auto& package = manifest.packages.front();
  crx->next_fp = package.fingerprint;

  // Resolve the urls by combining the base urls with the package names.
  for (const auto& crx_url : result.crx_urls) {
    const GURL url = crx_url.Resolve(package.name);
    if (url.is_valid())
      crx->crx_urls.push_back(url);
  }
  for (const auto& crx_diffurl : result.crx_diffurls) {
    const GURL url = crx_diffurl.Resolve(package.namediff);
    if (url.is_valid())
      crx->crx_diffurls.push_back(url);
  }

  crx->hash_sha256 = package.hash_sha256;
  crx->hashdiff_sha256 = package.hashdiff_sha256;

  ChangeItemState(crx, CrxUpdateItem::State::kCanUpdate);

  update_context_->queue.push(crx->id);
}

void ActionUpdateCheck::HandleUpdateCheckNoupdate(
    const UpdateResponse::Result& result,
    CrxUpdateItem* crx) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "No update for CRX: " << crx->id;
  ChangeItemState(crx, CrxUpdateItem::State::kNoUpdate);
}

void ActionUpdateCheck::HandleUpdateCheckError(
    const UpdateResponse::Result& result,
    CrxUpdateItem* crx) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Update error for CRX: " << crx->id << ", " << result.status;
  ChangeItemState(crx, CrxUpdateItem::State::kNoUpdate);
}

void ActionUpdateCheck::OnUpdateCheckFailed(int error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(error);

  VLOG(1) << "Update check failed." << error;

  ChangeAllItemsState(CrxUpdateItem::State::kChecking,
                      CrxUpdateItem::State::kNoUpdate);

  UpdateComplete(Error::UPDATE_CHECK_ERROR);
}

}  // namespace update_client
