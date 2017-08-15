// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_offer.h"

#include "components/exo/data_offer_delegate.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace exo {

DataOffer::DataOffer(DataOfferDelegate* delegate) : delegate_(delegate) {}

DataOffer::~DataOffer() {
  delegate_->OnDataOfferDestroying(this);
}

void DataOffer::Accept(const std::string& mime_type) {
  NOTIMPLEMENTED();
}

void DataOffer::Receive(const std::string& mime_type, base::ScopedFD fd) {
  NOTIMPLEMENTED();
}

void DataOffer::Finish() {
  NOTIMPLEMENTED();
}

void DataOffer::SetActions(const base::flat_set<DndAction>& dnd_actions,
                           DndAction preferred_action) {
  dnd_action_ = preferred_action;
  delegate_->OnAction(preferred_action);
}

void DataOffer::SetSourceActions(
    const base::flat_set<DndAction>& source_actions) {
  source_actions_ = source_actions;
  delegate_->OnSourceActions(source_actions);
}

void DataOffer::SetDropData(const ui::OSExchangeData& data) {
  DCHECK_EQ(0u, mime_types_.size());
  if (data.HasString())
    mime_types_.insert(ui::Clipboard::kMimeTypeText);
  for (const std::string& mime_type : mime_types_) {
    delegate_->OnOffer(mime_type);
  }
}

}  // namespace exo
