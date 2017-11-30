// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_source.h"

#include "components/exo/data_source_delegate.h"
#include "components/exo/data_source_observer.h"

namespace exo {

ScopedDataSource::ScopedDataSource(DataSource* data_source,
                                   DataSourceObserver* observer)
    : data_source_(data_source), observer_(observer) {
  data_source_->AddObserver(observer_);
}

ScopedDataSource::~ScopedDataSource() {
  data_source_->RemoveObserver(observer_);
}

DataSource::DataSource(DataSourceDelegate* delegate) : delegate_(delegate) {}

DataSource::~DataSource() {
  delegate_->OnDataSourceDestroying(this);
  for (DataSourceObserver& observer : observers_) {
    observer.OnDataSourceDestroying(this);
  }
}

void DataSource::AddObserver(DataSourceObserver* observer) {
  observers_.AddObserver(observer);
}

void DataSource::RemoveObserver(DataSourceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DataSource::Offer(const std::string& mime_type) {
  NOTIMPLEMENTED();
}

void DataSource::SetActions(const base::flat_set<DndAction>& dnd_actions) {
  NOTIMPLEMENTED();
}

void DataSource::Cancelled() {
  NOTIMPLEMENTED();
}

void DataSource::ReadData(ReadDataCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace exo
