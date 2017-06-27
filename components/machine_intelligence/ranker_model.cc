// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/machine_intelligence/ranker_model.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/machine_intelligence/proto/ranker_model.pb.h"

namespace machine_intelligence {

RankerModel::RankerModel() : proto_(base::MakeUnique<RankerModelProto>()) {}

RankerModel::~RankerModel() {}

// static
std::unique_ptr<RankerModel> RankerModel::FromString(const std::string& data) {
  auto model = base::MakeUnique<RankerModel>();
  if (!model->mutable_proto()->ParseFromString(data))
    return nullptr;
  return model;
}

bool RankerModel::IsExpired() const {
  if (!proto().has_metadata())
    return true;

  const auto& metadata = proto().metadata();

  // If the age of the model cannot be determined, presume it to be expired.
  if (!metadata.has_last_modified_sec())
    return true;

  // If the model has no set cache duration, then it never expires.
  if (!metadata.has_cache_duration_sec() || metadata.cache_duration_sec() == 0)
    return false;

  // Otherwise, a model is expired if its age exceeds the cache duration.
  base::Time last_modified =
      base::Time() + base::TimeDelta::FromSeconds(metadata.last_modified_sec());
  base::TimeDelta age = base::Time::Now() - last_modified;
  return age > base::TimeDelta::FromSeconds(metadata.cache_duration_sec());
}

const std::string& RankerModel::GetSourceURL() const {
  return proto_->metadata().source();
}

std::string RankerModel::SerializeAsString() const {
  return proto_->SerializeAsString();
}

}  // namespace machine_intelligence
