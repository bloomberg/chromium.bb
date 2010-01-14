// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_MANAGER_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_MANAGER_API_H_

#include "chrome/browser/extensions/extension_function.h"

class CopyBookmarkManagerFunction : public AsyncExtensionFunction {
 public:
  // Override BookmarkManagerFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.copy");
};

class CutBookmarkManagerFunction : public AsyncExtensionFunction {
 public:
  // Override BookmarkManagerFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.cut");
};

class PasteBookmarkManagerFunction : public AsyncExtensionFunction {
 public:
  // Override BookmarkManagerFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.paste");
};

class BookmarkManagerGetStringsFunction : public AsyncExtensionFunction {
 public:
  // Override BookmarkManagerFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.getStrings");
};


#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_MANAGER_API_H_
