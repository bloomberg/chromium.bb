// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_API_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_API_H_
#pragma once

#include <list>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class FilePath;

namespace base {
class ListValue;
}

// Observes BookmarkModel and then routes the notifications as events to
// the extension system.
class BookmarkExtensionEventRouter : public BookmarkModelObserver {
 public:
  explicit BookmarkExtensionEventRouter(BookmarkModel* model);
  virtual ~BookmarkExtensionEventRouter();

  void Init();

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
  virtual void BookmarkImportBeginning(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkImportEnding(BookmarkModel* model) OVERRIDE;

 private:
  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string& json_args);

  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkExtensionEventRouter);
};

class BookmarksFunction : public AsyncExtensionFunction,
                          public content::NotificationObserver {
 public:
  // AsyncExtensionFunction:
  virtual void Run() OVERRIDE;

  virtual bool RunImpl() = 0;

 protected:
  // Helper to get the bookmark id as int64 from the given string id.
  // Sets error_ to an error string if the given id string can't be parsed
  // as an int64. In case of error, doesn't change id and returns false.
  bool GetBookmarkIdAsInt64(const std::string& id_string, int64* id);

  // Helper that checks if bookmark editing is enabled. If it's not, this sets
  // error_ to the appropriate error string.
  bool EditBookmarksEnabled();

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
};

class GetBookmarksFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.get")
};

class GetBookmarkChildrenFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.getChildren")
};

class GetBookmarkRecentFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.getRecent")
};

class GetBookmarkTreeFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.getTree")
};

class GetBookmarkSubTreeFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.getSubTree")
};

class SearchBookmarksFunction : public BookmarksFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.search")
};

class RemoveBookmarkFunction : public BookmarksFunction {
 public:
  // Returns true on successful parse and sets invalid_id to true if conversion
  // from id string to int64 failed.
  static bool ExtractIds(const base::ListValue* args, std::list<int64>* ids,
                         bool* invalid_id);
  // BookmarksFunction:
  virtual bool RunImpl() OVERRIDE;
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.remove")
};

class RemoveTreeBookmarkFunction : public RemoveBookmarkFunction {
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.removeTree")
};

class CreateBookmarkFunction : public BookmarksFunction {
 public:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
  // BookmarksFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.create")
};

class MoveBookmarkFunction : public BookmarksFunction {
 public:
  static bool ExtractIds(const base::ListValue* args, std::list<int64>* ids,
                         bool* invalid_id);
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
  // BookmarksFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.move")
};

class UpdateBookmarkFunction : public BookmarksFunction {
 public:
  static bool ExtractIds(const base::ListValue* args, std::list<int64>* ids,
                         bool* invalid_id);
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
  virtual bool RunImpl();
 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.update")
};

class BookmarksIOFunction : public BookmarksFunction,
                            public SelectFileDialog::Listener {
 public:
  BookmarksIOFunction();
  virtual ~BookmarksIOFunction();

  virtual void FileSelected(const FilePath& path, int index, void* params) = 0;

  // SelectFileDialog::Listener:
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  void SelectFile(SelectFileDialog::Type type);

 private:
  void ShowSelectFileDialog(SelectFileDialog::Type type, FilePath default_path);

 protected:
  scoped_refptr<SelectFileDialog> select_file_dialog_;
};

class ImportBookmarksFunction : public BookmarksIOFunction {
 public:
  // BookmarkManagerIOFunction:
  virtual bool RunImpl() OVERRIDE;
  virtual void FileSelected(const FilePath& path, int index, void* params)
      OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.import");
};

class ExportBookmarksFunction : public BookmarksIOFunction {
 public:
  // BookmarkManagerIOFunction:
  virtual bool RunImpl() OVERRIDE;
  virtual void FileSelected(const FilePath& path, int index, void* params)
      OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("bookmarks.export");
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXTENSION_API_H_
