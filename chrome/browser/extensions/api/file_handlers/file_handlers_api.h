// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_FILE_HANDLERS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_FILE_HANDLERS_API_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"

class Profile;

namespace extensions {

class FileHandlersAPI : public ProfileKeyedAPI {
 public:
  explicit FileHandlersAPI(Profile* profile);
  virtual ~FileHandlersAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<FileHandlersAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<FileHandlersAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "FileHandlersAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_HANDLERS_FILE_HANDLERS_API_H_
