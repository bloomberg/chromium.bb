// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_SESSION_STORAGE_NAMESPACE_IMPL_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_SESSION_STORAGE_NAMESPACE_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/session_storage_namespace.h"
#include "webkit/dom_storage/dom_storage_types.h"

#ifdef ENABLE_NEW_DOM_STORAGE_BACKEND
// This class is replaced by a new implementation in
#include "content/browser/dom_storage/session_storage_namespace_impl_new.h"
#else

class DOMStorageContextImpl;

class SessionStorageNamespaceImpl
    : NON_EXPORTED_BASE(public content::SessionStorageNamespace) {
 public:
  explicit SessionStorageNamespaceImpl(DOMStorageContextImpl* context);

  int64 id() const { return id_; }

  // The session storage namespace parameter allows multiple render views and
  // tab contentses to share the same session storage (part of the WebStorage
  // spec) space. Passing in NULL simply allocates a new one which is often the
  // correct thing to do (especially in tests.
  SessionStorageNamespaceImpl* Clone();

 private:
  SessionStorageNamespaceImpl(DOMStorageContextImpl* context, int64 id);
  virtual ~SessionStorageNamespaceImpl();

  scoped_refptr<DOMStorageContextImpl> dom_storage_context_;

  // The session storage namespace id.
  int64 id_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageNamespaceImpl);
};

#endif  // ENABLE_NEW_DOM_STORAGE_BACKEND
#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_SESSION_STORAGE_NAMESPACE_IMPL_H_
