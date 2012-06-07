// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MANAGER_EXTENSION_API_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MANAGER_EXTENSION_API_H_
#pragma once

#include "chrome/browser/bookmarks/bookmark_extension_api.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"

struct BookmarkNodeData;
class Profile;
class TabContents;

namespace base {
class ListValue;
}

// Class that handles the chrome.experimental.bookmarkManager events.
class BookmarkManagerExtensionEventRouter
    : public BookmarkTabHelper::BookmarkDrag {
 public:
  BookmarkManagerExtensionEventRouter(Profile* profile, TabContents* tab);
  virtual ~BookmarkManagerExtensionEventRouter();

  // BookmarkTabHelper::BookmarkDrag interface
  virtual void OnDragEnter(const BookmarkNodeData& data) OVERRIDE;
  virtual void OnDragOver(const BookmarkNodeData& data) OVERRIDE;
  virtual void OnDragLeave(const BookmarkNodeData& data) OVERRIDE;
  virtual void OnDrop(const BookmarkNodeData& data) OVERRIDE;

  // The bookmark drag and drop data. This gets set after a drop was done on
  // the page. This returns NULL if no data is available.
  const BookmarkNodeData* GetBookmarkNodeData();

  // Clears the drag and drop data.
  void ClearBookmarkNodeData();

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const char* event_name, const base::ListValue* args);

  void DispatchDragEvent(const BookmarkNodeData& data, const char* event_name);

  Profile* profile_;
  TabContents* tab_;
  BookmarkNodeData bookmark_drag_data_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkManagerExtensionEventRouter);
};

class ClipboardBookmarkManagerFunction : public BookmarksFunction {
 protected:
  virtual ~ClipboardBookmarkManagerFunction() {}

  bool CopyOrCut(bool cut);
};

class CopyBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.copy");

 protected:
  virtual ~CopyBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CutBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.cut");

 protected:
  virtual ~CutBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class PasteBookmarkManagerFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.paste");

 protected:
  virtual ~PasteBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CanPasteBookmarkManagerFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.canPaste");

 protected:
  virtual ~CanPasteBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SortChildrenBookmarkManagerFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.sortChildren");

 protected:
  virtual ~SortChildrenBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarkManagerGetStringsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.getStrings");

 protected:
  virtual ~BookmarkManagerGetStringsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class StartDragBookmarkManagerFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.startDrag");

 protected:
  virtual ~StartDragBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DropBookmarkManagerFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.drop");

 protected:
  virtual ~DropBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetSubtreeBookmarkManagerFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.getSubtree");

 protected:
  virtual ~GetSubtreeBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CanEditBookmarkManagerFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.canEdit");

 protected:
  virtual ~CanEditBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class RecordLaunchBookmarkFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.recordLaunch")

 protected:
  virtual ~RecordLaunchBookmarkFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CanOpenNewWindowsBookmarkFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bookmarkManager.canOpenNewWindows");

 protected:
  virtual ~CanOpenNewWindowsBookmarkFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};


#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MANAGER_EXTENSION_API_H_
