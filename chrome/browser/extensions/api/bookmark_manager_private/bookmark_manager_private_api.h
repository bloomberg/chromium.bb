// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/undo/bookmark_undo_service.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

struct BookmarkNodeData;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class BookmarkManagerPrivateEventRouter : public BaseBookmarkModelObserver {
 public:
  BookmarkManagerPrivateEventRouter(content::BrowserContext* browser_context,
                                    BookmarkModel* bookmark_model);
  virtual ~BookmarkManagerPrivateEventRouter();

  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void OnWillChangeBookmarkMetaInfo(BookmarkModel* model,
                                            const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkMetaInfoChanged(BookmarkModel* model,
                                       const BookmarkNode* node) OVERRIDE;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     scoped_ptr<base::ListValue> event_args);

  // Remembers the previous meta info of a node before it was changed.
  BookmarkNode::MetaInfoMap prev_meta_info_;

  content::BrowserContext* browser_context_;
  BookmarkModel* bookmark_model_;
};

class BookmarkManagerPrivateAPI : public BrowserContextKeyedAPI,
                                  public EventRouter::Observer {
 public:
  explicit BookmarkManagerPrivateAPI(content::BrowserContext* browser_context);
  virtual ~BookmarkManagerPrivateAPI();

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<BookmarkManagerPrivateAPI>*
      GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<BookmarkManagerPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BookmarkManagerPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* browser_context_;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<BookmarkManagerPrivateEventRouter> event_router_;
};

// Class that handles the drag and drop related chrome.bookmarkManagerPrivate
// events. This class has one instance per bookmark manager tab.
class BookmarkManagerPrivateDragEventRouter
    : public BookmarkTabHelper::BookmarkDrag {
 public:
  BookmarkManagerPrivateDragEventRouter(Profile* profile,
                                        content::WebContents* web_contents);
  virtual ~BookmarkManagerPrivateDragEventRouter();

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
  void DispatchEvent(const std::string& event_name,
                     scoped_ptr<base::ListValue> args);

  Profile* profile_;
  content::WebContents* web_contents_;
  BookmarkNodeData bookmark_drag_data_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkManagerPrivateDragEventRouter);
};

class ClipboardBookmarkManagerFunction : public extensions::BookmarksFunction {
 protected:
  virtual ~ClipboardBookmarkManagerFunction() {}

  bool CopyOrCut(bool cut, const std::vector<std::string>& id_list);
};

class BookmarkManagerPrivateCopyFunction
    : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.copy",
                             BOOKMARKMANAGERPRIVATE_COPY)

 protected:
  virtual ~BookmarkManagerPrivateCopyFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateCutFunction
    : public ClipboardBookmarkManagerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.cut",
                             BOOKMARKMANAGERPRIVATE_CUT)

 protected:
  virtual ~BookmarkManagerPrivateCutFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivatePasteFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.paste",
                             BOOKMARKMANAGERPRIVATE_PASTE)

 protected:
  virtual ~BookmarkManagerPrivatePasteFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateCanPasteFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.canPaste",
                             BOOKMARKMANAGERPRIVATE_CANPASTE)

 protected:
  virtual ~BookmarkManagerPrivateCanPasteFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateSortChildrenFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.sortChildren",
                             BOOKMARKMANAGERPRIVATE_SORTCHILDREN)

 protected:
  virtual ~BookmarkManagerPrivateSortChildrenFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateGetStringsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.getStrings",
                             BOOKMARKMANAGERPRIVATE_GETSTRINGS)

 protected:
  virtual ~BookmarkManagerPrivateGetStringsFunction() {}

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
};

class BookmarkManagerPrivateStartDragFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.startDrag",
                             BOOKMARKMANAGERPRIVATE_STARTDRAG)

 protected:
  virtual ~BookmarkManagerPrivateStartDragFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateDropFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.drop",
                             BOOKMARKMANAGERPRIVATE_DROP)

 protected:
  virtual ~BookmarkManagerPrivateDropFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateGetSubtreeFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.getSubtree",
                             BOOKMARKMANAGERPRIVATE_GETSUBTREE)

 protected:
  virtual ~BookmarkManagerPrivateGetSubtreeFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateCanEditFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.canEdit",
                             BOOKMARKMANAGERPRIVATE_CANEDIT)

 protected:
  virtual ~BookmarkManagerPrivateCanEditFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateRecordLaunchFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.recordLaunch",
                             BOOKMARKMANAGERPRIVATE_RECORDLAUNCH)

 protected:
  virtual ~BookmarkManagerPrivateRecordLaunchFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateCreateWithMetaInfoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.createWithMetaInfo",
                             BOOKMARKMANAGERPRIVATE_CREATEWITHMETAINFO)

 protected:
  virtual ~BookmarkManagerPrivateCreateWithMetaInfoFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateGetMetaInfoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.getMetaInfo",
                             BOOKMARKMANAGERPRIVATE_GETMETAINFO)

 protected:
  virtual ~BookmarkManagerPrivateGetMetaInfoFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateSetMetaInfoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.setMetaInfo",
                             BOOKMARKMANAGERPRIVATE_SETMETAINFO)

 protected:
  virtual ~BookmarkManagerPrivateSetMetaInfoFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateUpdateMetaInfoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.updateMetaInfo",
                             BOOKMARKMANAGERPRIVATE_UPDATEMETAINFO)

 protected:
  virtual ~BookmarkManagerPrivateUpdateMetaInfoFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateCanOpenNewWindowsFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.canOpenNewWindows",
                             BOOKMARKMANAGERPRIVATE_CANOPENNEWWINDOWS)

 protected:
  virtual ~BookmarkManagerPrivateCanOpenNewWindowsFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateRemoveTreesFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.removeTrees",
                             BOOKMARKMANAGERPRIVATE_REMOVETREES)

 protected:
  virtual ~BookmarkManagerPrivateRemoveTreesFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateUndoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.undo",
                             BOOKMARKMANAGERPRIVATE_UNDO)

 protected:
  virtual ~BookmarkManagerPrivateUndoFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateRedoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.redo",
                             BOOKMARKMANAGERPRIVATE_REDO)

 protected:
  virtual ~BookmarkManagerPrivateRedoFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateGetUndoInfoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.getUndoInfo",
                             BOOKMARKMANAGERPRIVATE_UNDOINFO)

 protected:
  virtual ~BookmarkManagerPrivateGetUndoInfoFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

class BookmarkManagerPrivateGetRedoInfoFunction
    : public extensions::BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkManagerPrivate.getRedoInfo",
                             BOOKMARKMANAGERPRIVATE_REDOINFO)

 protected:
  virtual ~BookmarkManagerPrivateGetRedoInfoFunction() {}

  // ExtensionFunction:
  virtual bool RunOnReady() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARK_MANAGER_PRIVATE_BOOKMARK_MANAGER_PRIVATE_API_H_
