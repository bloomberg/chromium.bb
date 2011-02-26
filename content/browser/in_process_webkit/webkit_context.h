// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CONTEXT_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CONTEXT_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "content/browser/in_process_webkit/dom_storage_context.h"
#include "content/browser/in_process_webkit/indexed_db_context.h"

class Profile;

// There's one WebKitContext per profile.  Various DispatcherHost classes
// have a pointer to the Context to store shared state.  Unfortunately, this
// class has become a bit of a dumping ground for calls made on the UI thread
// that need to be proxied over to the WebKit thread.
//
// This class is created on the UI thread and accessed on the UI, IO, and WebKit
// threads.
class WebKitContext
    : public base::RefCountedThreadSafe<WebKitContext> {
 public:
  explicit WebKitContext(Profile* profile, bool clear_local_state_on_exit);

  const FilePath& data_path() const { return data_path_; }
  bool is_incognito() const { return is_incognito_; }

  DOMStorageContext* dom_storage_context() {
    return dom_storage_context_.get();
  }

  IndexedDBContext* indexed_db_context() {
    return indexed_db_context_.get();
  }

  void set_clear_local_state_on_exit(bool clear_local_state) {
    clear_local_state_on_exit_ = clear_local_state;
  }

#ifdef UNIT_TEST
  // For unit tests, allow specifying a DOMStorageContext directly so it can be
  // mocked.
  void set_dom_storage_context(DOMStorageContext* dom_storage_context) {
    dom_storage_context_.reset(dom_storage_context);
  }
#endif

  // Tells the DOMStorageContext to purge any memory it does not need.
  void PurgeMemory();

  // Tell all children (where applicable) to delete any objects that were
  // last modified on or after the following time.
  void DeleteDataModifiedSince(const base::Time& cutoff);

  // Delete the session storage namespace associated with this id.  Can be
  // called from any thread.
  void DeleteSessionStorageNamespace(int64 session_storage_namespace_id);

 private:
  friend class base::RefCountedThreadSafe<WebKitContext>;
  virtual ~WebKitContext();

  // Copies of profile data that can be accessed on any thread.
  const FilePath data_path_;
  const bool is_incognito_;

  // True if the destructors of context objects should delete their files.
  bool clear_local_state_on_exit_;

  scoped_ptr<DOMStorageContext> dom_storage_context_;

  scoped_ptr<IndexedDBContext> indexed_db_context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebKitContext);
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CONTEXT_H_
