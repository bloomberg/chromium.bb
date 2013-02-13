// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_

#include <string>

namespace base {
class DictionaryValue;
}

namespace extensions {

class WebstoreDataFetcherDelegate {
 public:
  // Invoked when the web store data request failed.
  virtual void OnWebstoreRequestFailure() = 0;

  // Invoked when the web store response parsing is successful. Delegate takes
  // ownership of |webstore_data|.
  virtual void OnWebstoreResponseParseSuccess(
      base::DictionaryValue* webstore_data) = 0;

  // Invoked when the web store response parsing is failed.
  virtual void OnWebstoreResponseParseFailure(const std::string& error) = 0;

 protected:
  virtual ~WebstoreDataFetcherDelegate() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_DELEGATE_H_
