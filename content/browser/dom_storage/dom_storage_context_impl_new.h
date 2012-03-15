// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_IMPL_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/time.h"
#include "content/public/browser/dom_storage_context.h"
#include "webkit/dom_storage/dom_storage_types.h"

#ifdef ENABLE_NEW_DOM_STORAGE_BACKEND

namespace dom_storage {
class DomStorageContext;
}

namespace quota {
class SpecialStoragePolicy;
}

// This is owned by BrowserContext (aka Profile) and encapsulates all
// per-profile dom storage state.
class CONTENT_EXPORT DOMStorageContextImpl :
    NON_EXPORTED_BASE(public content::DOMStorageContext) {
 public:
  // If |data_path| is empty, nothing will be saved to disk.
  DOMStorageContextImpl(const FilePath& data_path,
                        quota::SpecialStoragePolicy* special_storage_policy);
  virtual ~DOMStorageContextImpl();

  virtual base::SequencedTaskRunner* task_runner() const {return NULL;}

  // DOMStorageContext implementation:
  virtual std::vector<FilePath> GetAllStorageFiles() OVERRIDE;
  virtual FilePath GetFilePath(const string16& origin_id) const OVERRIDE;
  virtual void DeleteForOrigin(const string16& origin_id) OVERRIDE;
  virtual void DeleteLocalStorageFile(const FilePath& file_path) OVERRIDE;
  virtual void DeleteDataModifiedSince(const base::Time& cutoff) OVERRIDE;

  // Deletes all local storage files.
  void DeleteAllLocalStorageFiles();

  // Tells storage namespaces to purge any memory they do not need.
  void PurgeMemory();


  // TODO: merge/reconcile style differences in these two setters?
  void set_clear_local_state_on_exit(bool clear_local_state) {
    clear_local_state_on_exit_ = clear_local_state;
  }

  // Disables the exit-time deletion for all data (also session-only data).
  void SaveSessionState() {
    save_session_state_ = true;
  }

  dom_storage::DomStorageContext* context() const { return context_.get(); }

 private:
  // True if the destructor should delete its files.
  bool clear_local_state_on_exit_;

  // If true, nothing (not even session-only data) should be deleted on exit.
  bool save_session_state_;

  scoped_refptr<dom_storage::DomStorageContext> context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContextImpl);
};

#endif  // ENABLE_NEW_DOM_STORAGE_BACKEND
#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_IMPL_H_
