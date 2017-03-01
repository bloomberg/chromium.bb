// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/physical_web/webui/physical_web_base_message_handler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/physical_web/webui/physical_web_ui_constants.h"

namespace physical_web_ui {

PhysicalWebBaseMessageHandler::PhysicalWebBaseMessageHandler() {}

PhysicalWebBaseMessageHandler::~PhysicalWebBaseMessageHandler() = default;

void PhysicalWebBaseMessageHandler::RegisterMessages() {
  RegisterMessageCallback(
      kRequestNearbyUrls,
      base::BindRepeating(
          &PhysicalWebBaseMessageHandler::HandleRequestNearbyURLs,
          base::Unretained(this)));
  RegisterMessageCallback(kPhysicalWebItemClicked,
      base::BindRepeating(
          &PhysicalWebBaseMessageHandler::HandlePhysicalWebItemClicked,
          base::Unretained(this)));
}

void PhysicalWebBaseMessageHandler::HandleRequestNearbyURLs(
    const base::ListValue* args) {
  base::DictionaryValue results;

  std::unique_ptr<physical_web::MetadataList> metadata_list =
      GetPhysicalWebDataSource()->GetMetadataList();

  auto metadata = base::MakeUnique<base::ListValue>();
  int index = 0;
  for (const auto& metadata_list_item : *metadata_list) {
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

  results.Set(physical_web_ui::kMetadata, metadata.release());

  UMA_HISTOGRAM_EXACT_LINEAR("PhysicalWeb.TotalUrls.OnInitialDisplay",
                             (int)metadata_list->size(), 50);

  // Pass the list of Physical Web URL metadata to the WebUI. A jstemplate will
  // create a list view with an item for each URL.
  CallJavaScriptFunction(physical_web_ui::kReturnNearbyUrls, results);
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

} //  namespace physical_web_ui
