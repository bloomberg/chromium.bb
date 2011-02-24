// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_SESSION_STORAGE_NAMESPACE_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_SESSION_STORAGE_NAMESPACE_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"

class Profile;
class WebKitContext;

// This is a ref-counted class that represents a SessionStorageNamespace.
// On destruction it ensures that the storage namespace is destroyed.
// NOTE: That if we're shutting down, we don't strictly need to do this, but
//       it keeps valgrind happy.
class SessionStorageNamespace
    : public base::RefCountedThreadSafe<SessionStorageNamespace> {
 public:
  explicit SessionStorageNamespace(Profile* profile);

  int64 id() const { return id_; }

  // The session storage namespace parameter allows multiple render views and
  // tab contentses to share the same session storage (part of the WebStorage
  // spec) space. Passing in NULL simply allocates a new one which is often the
  // correct thing to do (especially in tests.
  SessionStorageNamespace* Clone();

 private:
  SessionStorageNamespace(WebKitContext* webkit_context, int64 id);

  friend class base::RefCountedThreadSafe<SessionStorageNamespace>;
  ~SessionStorageNamespace();

  scoped_refptr<WebKitContext> webkit_context_;

  // The session storage namespace id.
  int64 id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SessionStorageNamespace);
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_SESSION_STORAGE_NAMESPACE_H_
