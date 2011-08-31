// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/ref_counted.h"

template<typename T> struct DefaultSingletonTraits;

class PrintPreviewDataStore;
class RefCountedBytes;

// PrintPreviewDataService manages data stores for chrome://print requests.
// It owns the data store object and is responsible for freeing it.
class PrintPreviewDataService {
 public:
  static PrintPreviewDataService* GetInstance();

  // Get the data entry from PrintPreviewDataStore. |index| is zero-based or
  // |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to represent complete preview
  // data. Use |index| to retrieve a specific preview page data. |data| is set
  // to NULL if the requested page is not yet available.
  void GetDataEntry(const std::string& preview_ui_addr_str, int index,
                    scoped_refptr<RefCountedBytes>* data);

  // Set/Update the data entry in PrintPreviewDataStore. |index| is zero-based
  // or |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to represent complete
  // preview data. Use |index| to set/update a specific preview page data.
  // NOTE: PrintPreviewDataStore owns the data. Do not refcount |data| before
  // calling this function. It will be refcounted in PrintPreviewDataStore.
  void SetDataEntry(const std::string& preview_ui_addr_str, int index,
                    const RefCountedBytes* data);

  // Remove the corresponding PrintPreviewUI entry from the map.
  void RemoveEntry(const std::string& preview_ui_addr_str);

  // Returns the available draft page count.
  int GetAvailableDraftPageCount(const std::string& preview_ui_addr_str);

 private:
  friend struct DefaultSingletonTraits<PrintPreviewDataService>;

  // 1:1 relationship between PrintPreviewUI and data store object.
  // Key: Print preview UI address string.
  // Value: Print preview data store object.
  typedef std::map<std::string, scoped_refptr<PrintPreviewDataStore> >
      PreviewDataStoreMap;

  PrintPreviewDataService();
  virtual ~PrintPreviewDataService();

  PreviewDataStoreMap data_store_map_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDataService);
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_
