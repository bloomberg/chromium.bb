// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/physical_web/webui/physical_web_base_message_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/physical_web/webui/physical_web_ui_constants.h"

namespace physical_web_ui {

PhysicalWebBaseMessageHandler::PhysicalWebBaseMessageHandler()
    : data_source_(nullptr) {}

PhysicalWebBaseMessageHandler::~PhysicalWebBaseMessageHandler() {
  if (data_source_)
    data_source_->UnregisterListener(this);
}

void PhysicalWebBaseMessageHandler::OnFound(const GURL& url) {
  PushNearbyURLs();
}

void PhysicalWebBaseMessageHandler::OnLost(const GURL& url) {
  // do nothing
}

void PhysicalWebBaseMessageHandler::OnDistanceChanged(
    const GURL& url,
    double distance_estimate) {
  // do nothing
}

void PhysicalWebBaseMessageHandler::RegisterMessages() {
  RegisterMessageCallback(
      kPhysicalWebPageLoaded,
      base::BindRepeating(
          &PhysicalWebBaseMessageHandler::HandlePhysicalWebPageLoaded,
          base::Unretained(this)));

  RegisterMessageCallback(
      kPhysicalWebItemClicked,
      base::BindRepeating(
          &PhysicalWebBaseMessageHandler::HandlePhysicalWebItemClicked,
          base::Unretained(this)));

  data_source_ = GetPhysicalWebDataSource();
  if (data_source_)
    data_source_->RegisterListener(this, physical_web::OPPORTUNISTIC);
}

void PhysicalWebBaseMessageHandler::PushNearbyURLs() {
  base::DictionaryValue results;

  std::unique_ptr<physical_web::MetadataList> metadata_list =
      GetPhysicalWebDataSource()->GetMetadataList();

  // Append new metadata at the end of the list.
  for (const auto& metadata_list_item : *metadata_list) {
    const std::string& group_id = metadata_list_item.group_id;
    if (metadata_map_.find(group_id) == metadata_map_.end()) {
      ordered_group_ids_.push_back(group_id);
      metadata_map_.insert(std::make_pair(group_id, metadata_list_item));
    }
  }

  auto metadata = base::MakeUnique<base::ListValue>();
  int index = 0;
  for (const auto& group_id : ordered_group_ids_) {
    auto metadata_list_item = metadata_map_[group_id];
    auto metadata_item = base::MakeUnique<base::DictionaryValue>();
    metadata_item->SetString(physical_web_ui::kResolvedUrl,
                             metadata_list_item.resolved_url.spec());
    metadata_item->SetString(physical_web_ui::kPageInfoIcon,
                             metadata_list_item.icon_url.spec());
    metadata_item->SetString(physical_web_ui::kPageInfoTitle,
                             metadata_list_item.title);
    metadata_item->SetString(physical_web_ui::kPageInfoDescription,
                             metadata_list_item.description);
    // Add the item index so when an item is selected, the index can be recorded
    // in a UMA histogram.
    metadata_item->SetInteger(physical_web_ui::kIndex, index);
    metadata->Append(std::move(metadata_item));
    ++index;
  }

  results.Set(physical_web_ui::kMetadata, std::move(metadata));

  // Pass the list of Physical Web URL metadata to the WebUI. A jstemplate will
  // create a list view with an item for each URL.
  CallJavaScriptFunction(physical_web_ui::kPushNearbyUrls, results);
}

void PhysicalWebBaseMessageHandler::HandlePhysicalWebPageLoaded(
    const base::ListValue* args) {
  PushNearbyURLs();
  UMA_HISTOGRAM_EXACT_LINEAR("PhysicalWeb.TotalUrls.OnInitialDisplay",
                             (int)ordered_group_ids_.size(), 50);
}

void PhysicalWebBaseMessageHandler::HandlePhysicalWebItemClicked(
    const base::ListValue* args) {
  int index = 0;
  if (!args->GetInteger(0, &index)) {
    DLOG(ERROR) << "Invalid selection index";
    return;
  }

  // Record the index of the selected item.
  UMA_HISTOGRAM_EXACT_LINEAR("PhysicalWeb.WebUI.ListViewUrlPosition", index,
                             50);

  // Count the number of selections.
  base::RecordAction(
      base::UserMetricsAction("PhysicalWeb.WebUI.ListViewUrlSelected"));
}

}  //  namespace physical_web_ui
