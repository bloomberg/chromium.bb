// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
#define CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
#pragma once

#include "base/memory/ref_counted.h"

namespace content {

// This is a ref-counted class that represents a SessionStorageNamespace.
// On destruction it ensures that the storage namespace is destroyed.
// NOTE: That if we're shutting down, we don't strictly need to do this, but
//       it keeps valgrind happy.
class SessionStorageNamespace
    : public base::RefCountedThreadSafe<SessionStorageNamespace> {
 protected:
  friend class base::RefCountedThreadSafe<SessionStorageNamespace>;
  virtual ~SessionStorageNamespace() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
