// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_

#include <list>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class FilePath;

namespace base {
class ListValue;
}

namespace extensions {

// Observes BookmarkModel and then routes the notifications as events to
// the extension system.
class BookmarkEventRouter : public BookmarkModelObserver {
 public:
  explicit BookmarkEventRouter(BookmarkModel* model);
  virtual ~BookmarkEventRouter();

  // BookmarkModelObserver:
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;
  virtual void ExtensiveBookmarkChangesBeginning(BookmarkModel* model) OVERRIDE;
  virtual void ExtensiveBookmarkChangesEnded(BookmarkModel* model) OVERRIDE;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     scoped_ptr<base::ListValue> event_args);

  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkEventRouter);
};

class BookmarksAPI : public ProfileKeyedAPI,
                     public EventRouter::Observer {
 public:
  explicit BookmarksAPI(Profile* profile);
  virtual ~BookmarksAPI();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<BookmarksAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(
      const EventListenerInfo& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<BookmarksAPI>;

  Profile* profile_;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "BookmarksAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<BookmarkEventRouter> bookmark_event_router_;
};

class BookmarksFunction : public AsyncExtensionFunction,
                          public BaseBookmarkModelObserver {
 public:
  // AsyncExtensionFunction:
  virtual void Run() OVERRIDE;

 protected:
  BookmarksFunction();
  virtual ~BookmarksFunction();

  // Helper to get the bookmark id as int64 from the given string id.
  // Sets error_ to an error string if the given id string can't be parsed
  // as an int64. In case of error, doesn't change id and returns false.
  bool GetBookmarkIdAsInt64(const std::string& id_string, int64* id);

  // Helper that checks if bookmark editing is enabled. If it's not, this sets
  // error_ to the appropriate error string.
  bool EditBookmarksEnabled();

 private:
  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;

  BookmarkModel* bookmark_model_;
};

class BookmarksGetTreeFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.get", BOOKMARKS_GET)

 protected:
  virtual ~BookmarksGetTreeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksGetChildrenFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getChildren", BOOKMARKS_GETCHILDREN)

 protected:
  virtual ~BookmarksGetChildrenFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksGetFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getRecent", BOOKMARKS_GETRECENT)

 protected:
  virtual ~BookmarksGetFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksGetSubTreeFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getTree", BOOKMARKS_GETTREE)

 protected:
  virtual ~BookmarksGetSubTreeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksGetRecentFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getSubTree", BOOKMARKS_GETSUBTREE)

 protected:
  virtual ~BookmarksGetRecentFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksSearchFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.search", BOOKMARKS_SEARCH)

 protected:
  virtual ~BookmarksSearchFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksRemoveFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.remove", BOOKMARKS_REMOVE)

  // Returns true on successful parse and sets invalid_id to true if conversion
  // from id string to int64 failed.
  static bool ExtractIds(const base::ListValue* args,
                         std::list<int64>* ids,
                         bool* invalid_id);
  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;

 protected:
  virtual ~BookmarksRemoveFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksRemoveTreeFunction : public BookmarksRemoveFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.removeTree", BOOKMARKS_REMOVETREE)

 protected:
  virtual ~BookmarksRemoveTreeFunction() {}
};

class BookmarksCreateFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.create", BOOKMARKS_CREATE)

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;

 protected:
  virtual ~BookmarksCreateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksMoveFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.move", BOOKMARKS_MOVE)

  static bool ExtractIds(const base::ListValue* args,
                         std::list<int64>* ids,
                         bool* invalid_id);

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;

 protected:
  virtual ~BookmarksMoveFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksUpdateFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.update", BOOKMARKS_UPDATE)

  static bool ExtractIds(const base::ListValue* args,
                         std::list<int64>* ids,
                         bool* invalid_id);

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;

 protected:
  virtual ~BookmarksUpdateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksIOFunction : public BookmarksFunction,
                            public ui::SelectFileDialog::Listener {
 public:
  BookmarksIOFunction();

  virtual void FileSelected(const FilePath& path, int index, void* params) = 0;

  // ui::SelectFileDialog::Listener:
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  void SelectFile(ui::SelectFileDialog::Type type);

 protected:
  virtual ~BookmarksIOFunction();

 private:
  void ShowSelectFileDialog(
      ui::SelectFileDialog::Type type,
      const FilePath& default_path);

 protected:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

class BookmarksImportFunction : public BookmarksIOFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.import", BOOKMARKS_IMPORT)

  // BookmarkManagerIOFunction:
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

 private:
  virtual ~BookmarksImportFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BookmarksExportFunction : public BookmarksIOFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.export", BOOKMARKS_EXPORT)

  // BookmarkManagerIOFunction:
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

 private:
  virtual ~BookmarksExportFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_
