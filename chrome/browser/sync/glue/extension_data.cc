// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_data.h"

#include "base/logging.h"
#include "chrome/browser/sync/glue/extension_util.h"

namespace browser_sync {

ExtensionData ExtensionData::FromData(
    Source source, const sync_pb::ExtensionSpecifics& data) {
  DcheckIsExtensionSpecificsValid(data);
  ExtensionData extension_data;
  extension_data.merged_data_ = extension_data.source_data_[source] = data;
  DCHECK(AreExtensionSpecificsEqual(extension_data.merged_data(), data));
  DCHECK(!extension_data.NeedsUpdate(source));
  return extension_data;
}

ExtensionData::~ExtensionData() {}

const sync_pb::ExtensionSpecifics& ExtensionData::merged_data() const {
  DcheckIsExtensionSpecificsValid(merged_data_);
  return merged_data_;
}

bool ExtensionData::NeedsUpdate(Source source) const {
  SourceDataMap::const_iterator it = source_data_.find(source);
  return
      (it == source_data_.end()) ||
      !AreExtensionSpecificsEqual(it->second, merged_data_);
}

void ExtensionData::SetData(
    Source source, bool merge_user_properties,
    const sync_pb::ExtensionSpecifics& data) {
  DcheckIsExtensionSpecificsValid(data);
  source_data_[source] = data;
  MergeExtensionSpecifics(data, merge_user_properties, &merged_data_);
  DcheckIsExtensionSpecificsValid(merged_data_);
}

void ExtensionData::ResolveData(Source source) {
  source_data_[source] = merged_data_;
  DCHECK(!NeedsUpdate(source));
}

ExtensionData::ExtensionData() {}

}  // namespace browser_sync
