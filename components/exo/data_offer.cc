// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_offer.h"

#include "components/exo/data_offer_delegate.h"

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
  NOTIMPLEMENTED();
}

}  // namespace exo
