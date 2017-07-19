// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_source.h"

#include "components/exo/data_source_delegate.h"

namespace exo {

DataSource::DataSource(DataSourceDelegate* delegate) : delegate_(delegate) {}

DataSource::~DataSource() {
  delegate_->OnDataSourceDestroying(this);
}

void DataSource::Offer(const std::string& mime_type) {
  NOTIMPLEMENTED();
}

void DataSource::SetActions(const base::flat_set<DndAction>& dnd_actions) {
  NOTIMPLEMENTED();
}

}  // namespace exo
