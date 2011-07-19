// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_data_service.h"

#include "base/memory/singleton.h"

// static
PrintPreviewDataService* PrintPreviewDataService::GetInstance() {
  return Singleton<PrintPreviewDataService>::get();
}

PrintPreviewDataService::PrintPreviewDataService() {
}

PrintPreviewDataService::~PrintPreviewDataService() {
}

void PrintPreviewDataService::GetDataEntry(
    const std::string& preview_ui_addr_str,
    scoped_refptr<RefCountedBytes>* data_bytes) {
  PreviewDataSourceMap::iterator it = data_src_map_.find(preview_ui_addr_str);
  if (it != data_src_map_.end())
    *data_bytes = it->second.get();
}

void PrintPreviewDataService::SetDataEntry(
    const std::string& preview_ui_addr_str, const RefCountedBytes* data_bytes) {
  RemoveEntry(preview_ui_addr_str);
  data_src_map_[preview_ui_addr_str] = const_cast<RefCountedBytes*>(data_bytes);
}

void PrintPreviewDataService::RemoveEntry(
    const std::string& preview_ui_addr_str) {
  PreviewDataSourceMap::iterator it = data_src_map_.find(preview_ui_addr_str);
  if (it != data_src_map_.end())
    data_src_map_.erase(it);
}
