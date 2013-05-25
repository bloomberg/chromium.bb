// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/web_ui_controller.h"
#include "net/base/net_errors.h"

namespace base { class ListValue; }

namespace content {

class DownloadItem;
class IndexedDBContextImpl;
class StoragePartition;

// The implementation for the chrome://indexeddb-internals page.
class IndexedDBInternalsUI : public WebUIController {
 public:
  explicit IndexedDBInternalsUI(WebUI* web_ui);
  virtual ~IndexedDBInternalsUI();

 private:
  typedef std::vector<scoped_refptr<IndexedDBContext> > ContextList;
  void GetAllOrigins(const base::ListValue* args);
  void GetAllOriginsOnWebkitThread(
      const scoped_ptr<ContextList> contexts,
      const scoped_ptr<std::vector<base::FilePath> > context_paths);
  void OnOriginsReady(scoped_ptr<std::vector<IndexedDBInfo> > origins,
                      const base::FilePath& path);

  static void AddContextFromStoragePartition(ContextList* contexts,
                                             std::vector<base::FilePath>* paths,
                                             StoragePartition* partition);

  void DownloadOriginData(const base::ListValue* args);
  void DownloadOriginDataOnWebkitThread(
      const base::FilePath& partition_path,
      const scoped_refptr<IndexedDBContextImpl> context,
      const GURL& origin_url);
  void OnDownloadDataReady(const base::FilePath& partition_path,
                           const GURL& origin_url,
                           const base::FilePath temp_path,
                           const base::FilePath zip_path);
  void OnDownloadStarted(const base::FilePath& partition_path,
                         const GURL& origin_url,
                         const base::FilePath& temp_path,
                         content::DownloadItem* item,
                         net::Error error);

  DISALLOW_COPY_AND_ASSIGN(IndexedDBInternalsUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
