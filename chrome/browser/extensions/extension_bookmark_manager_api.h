// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_MANAGER_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_MANAGER_API_H_

#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/shell_dialogs.h"

class BookmarkNode;

class ClipboardBookmarkManagerFunction : public BookmarksFunction {
 protected:
  bool CopyOrCut(bool cut);
  // Returns a single bookmark node from the ID passed as the first argument.
  const BookmarkNode* GetNodeFromArguments();
};

class CopyBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  // Override ClipboardBookmarkManagerFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.copy");
};

class CutBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  // Override ClipboardBookmarkManagerFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.cut");
};

class PasteBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  // Override ClipboardBookmarkManagerFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.paste");
};

class CanPasteBookmarkManagerFunction
    : public ClipboardBookmarkManagerFunction {
 public:
  // Override ClipboardBookmarkManagerFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.canPaste");
};

class BookmarkManagerIOFunction : public BookmarksFunction,
                                  public SelectFileDialog::Listener {
 public:
  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const FilePath& path, int index, void* params) = 0;
  void MultiFilesSelected(const std::vector<FilePath>& files, void* params);
  void FileSelectionCanceled(void* params);
  void SelectFile(SelectFileDialog::Type type);

 protected:
  scoped_refptr<SelectFileDialog> select_file_dialog_;
};

class ImportBookmarksFunction : public BookmarkManagerIOFunction {
 public:
  // Override BookmarkManagerIOFunction.
  bool RunImpl();
  void FileSelected(const FilePath& path, int index, void* params);

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.import");
};

class ExportBookmarksFunction : public BookmarkManagerIOFunction {
 public:
  // Override BookmarkManagerIOFunction.
  bool RunImpl();
  void FileSelected(const FilePath& path, int index, void* params);

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.export");
};

class BookmarkManagerGetStringsFunction : public AsyncExtensionFunction {
 public:
  // Override AsyncExtensionFunction.
  virtual bool RunImpl();

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.getStrings");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BOOKMARK_MANAGER_API_H_
