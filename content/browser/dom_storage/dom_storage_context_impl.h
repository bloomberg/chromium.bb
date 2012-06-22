// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_IMPL_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "content/public/browser/dom_storage_context.h"

class FilePath;

namespace quota {
class SpecialStoragePolicy;
}

// This is owned by BrowserContext (aka Profile) and encapsulates all
// per-profile dom storage state.
class CONTENT_EXPORT DOMStorageContextImpl :
    NON_EXPORTED_BASE(public content::DOMStorageContext),
    public base::RefCountedThreadSafe<DOMStorageContextImpl> {
 public:
  // If |data_path| is empty, nothing will be saved to disk.
  DOMStorageContextImpl(const FilePath& data_path,
                        quota::SpecialStoragePolicy* special_storage_policy);

  // DOMStorageContext implementation.
  virtual void GetUsageInfo(const GetUsageInfoCallback& callback) OVERRIDE;
  virtual void DeleteOrigin(const GURL& origin) OVERRIDE;
  virtual scoped_refptr<content::SessionStorageNamespace>
      RecreateSessionStorage(const std::string& persistent_id) OVERRIDE;

  // Called to free up memory that's not strictly needed.
  void PurgeMemory();

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState();

  // Called when the BrowserContext/Profile is going away.
  void Shutdown();

 private:
  friend class DOMStorageMessageFilter;  // for access to context()
  friend class SessionStorageNamespaceImpl;  // ditto
  friend class base::RefCountedThreadSafe<DOMStorageContextImpl>;

  virtual ~DOMStorageContextImpl();
  dom_storage::DomStorageContext* context() const { return context_.get(); }

  scoped_refptr<dom_storage::DomStorageContext> context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContextImpl);
};

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_IMPL_H_
