// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
#define CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace content {

// This is a ref-counted class that represents a SessionStorageNamespace.
// On destruction it ensures that the storage namespace is destroyed.
class SessionStorageNamespace
    : public base::RefCountedThreadSafe<SessionStorageNamespace> {
 public:
  SessionStorageNamespace() {}

  // Returns the ID of the |SessionStorageNamespace|. The ID is unique among all
  // SessionStorageNamespace objects, but not unique across browser runs.
  virtual int64 id() const = 0;

  // Returns the persistent ID for the |SessionStorageNamespace|. The ID is
  // unique across browser runs.
  virtual const std::string& persistent_id() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<SessionStorageNamespace>;
  virtual ~SessionStorageNamespace() {}

  DISALLOW_COPY_AND_ASSIGN(SessionStorageNamespace);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
