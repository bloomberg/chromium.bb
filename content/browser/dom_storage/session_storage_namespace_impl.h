// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/session_storage_namespace.h"

class DOMStorageContextImpl;

namespace dom_storage {
class DomStorageSession;
}

class SessionStorageNamespaceImpl
    : NON_EXPORTED_BASE(public content::SessionStorageNamespace) {
 public:
  explicit SessionStorageNamespaceImpl(DOMStorageContextImpl* context);
  SessionStorageNamespaceImpl(DOMStorageContextImpl* context,
                              int64 namepace_id_to_clone);
  int64 id() const;
  SessionStorageNamespaceImpl* Clone();

 private:
  explicit SessionStorageNamespaceImpl(dom_storage::DomStorageSession* clone);
  virtual ~SessionStorageNamespaceImpl();

  scoped_refptr<dom_storage::DomStorageSession> session_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageNamespaceImpl);
};

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
