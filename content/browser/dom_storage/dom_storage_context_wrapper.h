// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/dom_storage_context.h"

namespace base {
class FilePath;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace content {

class DOMStorageContextImpl;

// This is owned by BrowserContext (aka Profile) and encapsulates all
// per-profile dom storage state.
class CONTENT_EXPORT DOMStorageContextWrapper :
    NON_EXPORTED_BASE(public DOMStorageContext),
    public base::RefCountedThreadSafe<DOMStorageContextWrapper> {
 public:
  // If |data_path| is empty, nothing will be saved to disk.
  DOMStorageContextWrapper(
      const base::FilePath& data_path,
      storage::SpecialStoragePolicy* special_storage_policy);

  // DOMStorageContext implementation.
  void GetLocalStorageUsage(
      const GetLocalStorageUsageCallback& callback) override;
  void GetSessionStorageUsage(
      const GetSessionStorageUsageCallback& callback) override;
  void DeleteLocalStorage(const GURL& origin) override;
  void DeleteSessionStorage(const SessionStorageUsageInfo& usage_info) override;
  void SetSaveSessionStorageOnDisk() override;
  scoped_refptr<SessionStorageNamespace> RecreateSessionStorage(
      const std::string& persistent_id) override;
  void StartScavengingUnusedSessionStorage() override;

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState();

  // Called when the BrowserContext/Profile is going away.
  void Shutdown();

  void Flush();

 private:
  friend class DOMStorageMessageFilter;  // for access to context()
  friend class SessionStorageNamespaceImpl;  // ditto
  friend class base::RefCountedThreadSafe<DOMStorageContextWrapper>;

  ~DOMStorageContextWrapper() override;
  DOMStorageContextImpl* context() const { return context_.get(); }

  scoped_refptr<DOMStorageContextImpl> context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
