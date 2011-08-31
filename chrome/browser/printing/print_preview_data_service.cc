// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_data_service.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "printing/print_job_constants.h"

// PrintPreviewDataStore stores data for preview workflow and preview printing
// workflow.
//
// NOTE:
//   This class stores a list of PDFs. The list |index| is zero-based and can
// be |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to represent complete preview
// document. The PDF stored at |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| is
// optimized with font subsetting, compression, etc. PDF's stored at all other
// indices are unoptimized.
//
// PrintPreviewDataStore owns the data and is responsible for freeing it when
// either:
//    a) There is a new data.
//    b) When PrintPreviewDataStore is destroyed.
//
class PrintPreviewDataStore : public base::RefCounted<PrintPreviewDataStore> {
 public:
  PrintPreviewDataStore() {}

  // Get the preview page for the specified |index|.
  void GetPreviewDataForIndex(int index, scoped_refptr<RefCountedBytes>* data) {
    if (index != printing::COMPLETE_PREVIEW_DOCUMENT_INDEX &&
        index < printing::FIRST_PAGE_INDEX) {
      return;
    }

    PreviewPageDataMap::iterator it = page_data_map_.find(index);
    if (it != page_data_map_.end())
      *data = it->second.get();
  }

  // Set/Update the preview data entry for the specified |index|.
  void SetPreviewDataForIndex(int index, const RefCountedBytes* data) {
    if (index != printing::COMPLETE_PREVIEW_DOCUMENT_INDEX &&
        index < printing::FIRST_PAGE_INDEX) {
      return;
    }

    page_data_map_[index] = const_cast<RefCountedBytes*>(data);
  }

  // Returns the available draft page count.
  int GetAvailableDraftPageCount() {
    int page_data_map_size = page_data_map_.size();
    if (page_data_map_.find(printing::COMPLETE_PREVIEW_DOCUMENT_INDEX) !=
        page_data_map_.end()) {
      page_data_map_size--;
    }
    return page_data_map_size;
  }

 private:
  friend class base::RefCounted<PrintPreviewDataStore>;

  // 1:1 relationship between page index and its associated preview data.
  // Key: Page index is zero-based and can be
  // |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to represent complete preview
  // document.
  // Value: Preview data.
  typedef std::map<int, scoped_refptr<RefCountedBytes> > PreviewPageDataMap;

  ~PrintPreviewDataStore() {}

  PreviewPageDataMap page_data_map_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDataStore);
};

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
    int index,
    scoped_refptr<RefCountedBytes>* data_bytes) {
  *data_bytes = NULL;
  PreviewDataStoreMap::iterator it = data_store_map_.find(preview_ui_addr_str);
  if (it != data_store_map_.end())
    it->second->GetPreviewDataForIndex(index, data_bytes);
}

void PrintPreviewDataService::SetDataEntry(
    const std::string& preview_ui_addr_str,
    int index,
    const RefCountedBytes* data_bytes) {
  PreviewDataStoreMap::iterator it = data_store_map_.find(preview_ui_addr_str);
  if (it == data_store_map_.end())
    data_store_map_[preview_ui_addr_str] = new PrintPreviewDataStore();

  data_store_map_[preview_ui_addr_str]->SetPreviewDataForIndex(index,
                                                               data_bytes);
}

void PrintPreviewDataService::RemoveEntry(
    const std::string& preview_ui_addr_str) {
  PreviewDataStoreMap::iterator it = data_store_map_.find(preview_ui_addr_str);
  if (it != data_store_map_.end())
    data_store_map_.erase(it);
}

int PrintPreviewDataService::GetAvailableDraftPageCount(
    const std::string& preview_ui_addr_str) {
  if (data_store_map_.find(preview_ui_addr_str) != data_store_map_.end())
    return data_store_map_[preview_ui_addr_str]->GetAvailableDraftPageCount();
  return 0;
}
