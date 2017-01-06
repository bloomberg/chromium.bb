// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/physical_web/webui/physical_web_base_message_handler.h"

#include "base/bind.h"
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

  std::unique_ptr<base::ListValue> metadata =
      GetPhysicalWebDataSource()->GetMetadata();

  // Add item indices. When an item is selected, the index of the selected item
  // is recorded in a UMA histogram.
  for (size_t i = 0; i < metadata->GetSize(); i++) {
    base::DictionaryValue* metadata_item = nullptr;
    metadata->GetDictionary(i, &metadata_item);
    metadata_item->SetInteger(physical_web_ui::kIndex, static_cast<int>(i));
  }

  results.Set(physical_web_ui::kMetadata, metadata.release());

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
