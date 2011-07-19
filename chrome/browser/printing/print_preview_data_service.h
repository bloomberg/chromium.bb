// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"

template<typename T> struct DefaultSingletonTraits;

// PrintPreviewDataService manages data for chrome://print requests.
//
// PrintPreviewDataService owns the data and is responsible for freeing it when
// either:
//    a) There is a new data.
//    b) When PrintPreviewDataService is destroyed.
//
class PrintPreviewDataService {
 public:
  static PrintPreviewDataService* GetInstance();

  // Get the data entry from PreviewDataSourceMap.
  void GetDataEntry(const std::string& preview_ui_addr_str,
                    scoped_refptr<RefCountedBytes>* data);

  // Set/Update the data entry in PreviewDataSourceMap.
  void SetDataEntry(const std::string& preview_ui_addr_str,
                    const RefCountedBytes* data);

  // Remove the corresponding PrintPreviewUI entry from the map.
  void RemoveEntry(const std::string& preview_ui_addr_str);

 private:
  friend struct DefaultSingletonTraits<PrintPreviewDataService>;

  // 1:1 relationship between PrintPreviewUI and preview data.
  // Key: Print preview UI address string.
  // Value: Preview data.
  typedef std::map<std::string, scoped_refptr<RefCountedBytes> >
      PreviewDataSourceMap;

  PrintPreviewDataService();
  virtual ~PrintPreviewDataService();

  PreviewDataSourceMap data_src_map_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDataService);
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_
