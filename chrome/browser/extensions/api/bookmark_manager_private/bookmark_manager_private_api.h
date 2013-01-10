// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_

#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"

struct BookmarkNodeData;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {

// Class that handles the chrome.bookmarkManagerPrivate events.
class BookmarkManagerPrivateEventRouter
    : public BookmarkTabHelper::BookmarkDrag {
 public:
  BookmarkManagerPrivateEventRouter(Profile* profile,
                                    content::WebContents* web_contents);
  virtual ~BookmarkManagerPrivateEventRouter();

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
  void DispatchEvent(const char* event_name,
                     scoped_ptr<base::ListValue> args);

  void DispatchDragEvent(const BookmarkNodeData& data, const char* event_name);

  Profile* profile_;
  content::WebContents* web_contents_;
  BookmarkNodeData bookmark_drag_data_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkManagerPrivateEventRouter);
};

class ClipboardBookmarkManagerFunction : public extensions::BookmarksFunction {
 protected:
  virtual ~ClipboardBookmarkManagerFunction() {}

  bool CopyOrCut(bool cut);
};

class CopyBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.copy");

 protected:
  virtual ~CopyBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CutBookmarkManagerFunction : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.cut");

 protected:
  virtual ~CutBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class PasteBookmarkManagerFunction : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.paste");

 protected:
  virtual ~PasteBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CanPasteBookmarkManagerFunction : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.canPaste");

 protected:
  virtual ~CanPasteBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SortChildrenBookmarkManagerFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.sortChildren");

 protected:
  virtual ~SortChildrenBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarkManagerGetStringsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.getStrings");

 protected:
  virtual ~BookmarkManagerGetStringsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class StartDragBookmarkManagerFunction : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.startDrag");

 protected:
  virtual ~StartDragBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DropBookmarkManagerFunction : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.drop");

 protected:
  virtual ~DropBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetSubtreeBookmarkManagerFunction : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.getSubtree");

 protected:
  virtual ~GetSubtreeBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CanEditBookmarkManagerFunction : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.canEdit");

 protected:
  virtual ~CanEditBookmarkManagerFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class RecordLaunchBookmarkFunction : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.recordLaunch")

 protected:
  virtual ~RecordLaunchBookmarkFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class CanOpenNewWindowsBookmarkFunction : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarkManagerPrivate.canOpenNewWindows");

 protected:
  virtual ~CanOpenNewWindowsBookmarkFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_
