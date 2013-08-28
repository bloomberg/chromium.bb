// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file only provides getStrings() as the .cc file for it is big.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_STRINGS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_STRINGS_H_

#include "chrome/browser/extensions/extension_function.h"

namespace file_manager {

// Implements the chrome.fileBrowserPrivate.getStrings method.
// Used to get strings for the file manager from JavaScript.
class GetStringsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getStrings",
                             FILEBROWSERPRIVATE_GETSTRINGS)

  GetStringsFunction();

 protected:
  virtual ~GetStringsFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_STRINGS_H_
