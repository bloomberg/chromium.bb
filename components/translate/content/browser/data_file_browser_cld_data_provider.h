// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_DATA_FILE_BROWSER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_DATA_FILE_BROWSER_CLD_DATA_PROVIDER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/content/browser/browser_cld_data_provider.h"

namespace translate {

class DataFileBrowserCldDataProvider : public BrowserCldDataProvider {
 public:
  explicit DataFileBrowserCldDataProvider(content::WebContents*);
  virtual ~DataFileBrowserCldDataProvider();
  // BrowserCldDataProvider implementations:
  virtual bool OnMessageReceived(const IPC::Message&) OVERRIDE;
  virtual void OnCldDataRequest() OVERRIDE;
  virtual void SendCldDataResponse() OVERRIDE;

  // Sets the data file that this data provider will use to fulfill requests.
  // This method does nothing if the specified path is equal to the path that
  // is already configured. Otherwise, the specified path is cached and
  // subsequent requests for data will attempt to load CLD data from the file
  // at the specified path.
  // This method is threadsafe.
  static void SetCldDataFilePath(const base::FilePath& filePath);

  // Returns the path most recently set by SetDataFilePath. The initial value
  // prior to any such call is the empty path.
  // This method is threadsafe.
  static base::FilePath GetCldDataFilePath();

 private:
  void SendCldDataResponseInternal(const base::File*,
                                   const uint64,
                                   const uint64);
  static void OnCldDataRequestInternal();

  content::WebContents* web_contents_;
  scoped_ptr<base::WeakPtrFactory<DataFileBrowserCldDataProvider> >
      weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataFileBrowserCldDataProvider);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_DATA_FILE_BROWSER_CLD_DATA_PROVIDER_H_
