// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_READING_LIST_PRIVATE_READING_LIST_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_READING_LIST_PRIVATE_READING_LIST_PRIVATE_API_H_

#include "base/values.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/reading_list_private.h"

namespace content {
class WebContents;
}

namespace extensions {

class ReadingListPrivateAddEntryFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingListPrivate.addEntry",
                             READINGLISTPRIVATE_ADDENTRY)

 protected:
  virtual ~ReadingListPrivateAddEntryFunction() {}

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
};

class ReadingListPrivateRemoveEntryFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingListPrivate.removeEntry",
                             READINGLISTPRIVATE_REMOVEENTRY)

 protected:
  virtual ~ReadingListPrivateRemoveEntryFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class ReadingListPrivateGetEntriesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readingListPrivate.getEntries",
                             READINGLISTPRIVATE_GETENTRIES)

 protected:
  virtual ~ReadingListPrivateGetEntriesFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_READING_LIST_PRIVATE_READING_LIST_PRIVATE_API_H_
