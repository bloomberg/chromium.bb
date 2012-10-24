// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/session_storage_namespace.h"

namespace dom_storage {
class DomStorageSession;
}

namespace content {
class DOMStorageContextImpl;

class SessionStorageNamespaceImpl
    : NON_EXPORTED_BASE(public SessionStorageNamespace) {
 public:
  // Constructs a |SessionStorageNamespaceImpl| and allocates new IDs for it.
  //
  // The CONTENT_EXPORT allows TestRenderViewHost to instantiate these.
  CONTENT_EXPORT explicit SessionStorageNamespaceImpl(
      DOMStorageContextImpl* context);

  // Constructs a |SessionStorageNamespaceImpl| by cloning
  // |namespace_to_clone|. Allocates new IDs for it.
  SessionStorageNamespaceImpl(DOMStorageContextImpl* context,
                              int64 namepace_id_to_clone);

  // Constructs a |SessionStorageNamespaceImpl| and assigns |persistent_id|
  // to it. Allocates a new non-persistent ID.
  SessionStorageNamespaceImpl(DOMStorageContextImpl* context,
                              const std::string& persistent_id);

  // SessionStorageNamespace implementation.
  virtual int64 id() const OVERRIDE;
  virtual const std::string& persistent_id() const OVERRIDE;
  virtual void SetShouldPersist(bool should_persist) OVERRIDE;
  virtual bool should_persist() const OVERRIDE;

  SessionStorageNamespaceImpl* Clone();
  bool IsFromContext(DOMStorageContextImpl* context);

 private:
  explicit SessionStorageNamespaceImpl(dom_storage::DomStorageSession* clone);
  virtual ~SessionStorageNamespaceImpl();

  scoped_refptr<dom_storage::DomStorageSession> session_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageNamespaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
