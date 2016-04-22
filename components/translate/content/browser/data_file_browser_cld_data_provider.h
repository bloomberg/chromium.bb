// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOT DEAD CODE!
// This code isn't dead, even if it isn't currently being used. Please refer to:
// https://www.chromium.org/developers/how-tos/compact-language-detector-cld-data-source-configuration

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_DATA_FILE_BROWSER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_DATA_FILE_BROWSER_CLD_DATA_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/content/browser/browser_cld_data_provider.h"

namespace content {
class WebContents;
}

namespace translate {

class DataFileBrowserCldDataProvider : public BrowserCldDataProvider {
 public:
  explicit DataFileBrowserCldDataProvider(content::WebContents*);
  ~DataFileBrowserCldDataProvider() override;

  // BrowserCldDataProvider implementations:
  bool OnMessageReceived(const IPC::Message&) override;
  void OnCldDataRequest() override;
  void SendCldDataResponse() override;

 private:
  void SendCldDataResponseInternal(const base::File*,
                                   const uint64_t,
                                   const uint64_t);
  static void OnCldDataRequestInternal();

  content::WebContents* web_contents_;
  std::unique_ptr<base::WeakPtrFactory<DataFileBrowserCldDataProvider>>
      weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataFileBrowserCldDataProvider);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_DATA_FILE_BROWSER_CLD_DATA_PROVIDER_H_
