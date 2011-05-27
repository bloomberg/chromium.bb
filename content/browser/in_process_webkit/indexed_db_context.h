// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CONTEXT_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CONTEXT_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_thread.h"

class GURL;
class FilePath;
class WebKitContext;

namespace WebKit {
class WebIDBFactory;
}

namespace base {
class MessageLoopProxy;
}

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

class IndexedDBContext :
    public base::RefCountedThreadSafe<IndexedDBContext,
                                      BrowserThread::DeleteOnWebKitThread> {
 public:
  IndexedDBContext(WebKitContext* webkit_context,
                   quota::SpecialStoragePolicy* special_storage_policy,
                   quota::QuotaManagerProxy* quota_manager_proxy,
                   base::MessageLoopProxy* webkit_thread_loop);

  ~IndexedDBContext();

  WebKit::WebIDBFactory* GetIDBFactory();

  // The indexed db directory.
  static const FilePath::CharType kIndexedDBDirectory[];

  // The indexed db file extension.
  static const FilePath::CharType kIndexedDBExtension[];

  // Get the file name of the indexed db file for the given origin.
  FilePath GetIndexedDBFilePath(const string16& origin_id) const;

  void set_clear_local_state_on_exit(bool clear_local_state) {
    clear_local_state_on_exit_ = clear_local_state;
  }

  // Deletes a single indexed db file.
  void DeleteIndexedDBFile(const FilePath& file_path);

  // Deletes all indexed db files for the given origin.
  void DeleteIndexedDBForOrigin(const string16& origin_id);

  // Does a particular origin get unlimited storage?
  bool IsUnlimitedStorageGranted(const GURL& origin) const;

#ifdef UNIT_TEST
  // For unit tests allow to override the |data_path_|.
  void set_data_path(const FilePath& data_path) { data_path_ = data_path; }
#endif

 private:
  scoped_ptr<WebKit::WebIDBFactory> idb_factory_;

  // Path where the indexed db data is stored
  FilePath data_path_;

  // True if the destructor should delete its files.
  bool clear_local_state_on_exit_;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBContext);
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CONTEXT_H_
