// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/environment_data.h"

#include "base/stl_util.h"

namespace mojo {
namespace common {

// static
EnvironmentData* EnvironmentData::instance_ = NULL;

EnvironmentData::EnvironmentData() {
  DCHECK(!instance_);
  instance_ = this;
}

EnvironmentData::~EnvironmentData() {
  instance_ = NULL;
  DataMap data_map;
  data_map.swap(data_map_);
  STLDeleteContainerPairSecondPointers(data_map.begin(), data_map.end());
}

// static
EnvironmentData* EnvironmentData::GetInstance() {
  return instance_;
}

void EnvironmentData::SetData(const void* key, scoped_ptr<Data> data) {
  Data* old = NULL;
  {
    base::AutoLock auto_lock(data_lock_);
    old = data_map_[key];
    if (data)
      data_map_[key] = data.release();
    else
      data_map_.erase(key);
  }
  delete old;
}

EnvironmentData::Data* EnvironmentData::GetData(const void* key) {
  base::AutoLock auto_lock(data_lock_);
  return data_map_.count(key) > 0 ? data_map_[key] : NULL;
}

}  // namespace common
}  // namespace mojo
