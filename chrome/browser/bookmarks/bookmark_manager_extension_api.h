// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
class TabContentsWrapper;

namespace base {
class ListValue;
}

// Class that handles the chrome.experimental.bookmarkManager events.
class BookmarkManagerExtensionEventRouter
    : public BookmarkTabHelper::BookmarkDrag {
 public:
  BookmarkManagerExtensionEventRouter(Profile* profile,
                                      TabContentsWrapper* tab);
  virtual ~BookmarkManagerExtensionEventRouter();

  // RenderViewHostDelegate::BookmarkDrag interface
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
  TabContentsWrapper* tab_;
  BookmarkNodeData bookmark_drag_data_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkManagerExtensionEventRouter);
};

class ClipboardBookmarkManagerFunction : public BookmarksFunction {
 protected:
  bool CopyOrCut(bool cut);
};

class CopyBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.copy");
};

class CutBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.cut");
};

class PasteBookmarkManagerFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.paste");
};

class CanPasteBookmarkManagerFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.canPaste");
};

class SortChildrenBookmarkManagerFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.sortChildren");
};

class BookmarkManagerGetStringsFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.getStrings");
};

class StartDragBookmarkManagerFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.startDrag");
};

class DropBookmarkManagerFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.drop");
};

class GetSubtreeBookmarkManagerFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.getSubtree");
};

class CanEditBookmarkManagerFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.canEdit");
};

class RecordLaunchBookmarkFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bookmarkManager.recordLaunch")
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MANAGER_EXTENSION_API_H_
