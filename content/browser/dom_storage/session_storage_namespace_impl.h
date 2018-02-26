// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/session_storage_namespace.h"

namespace content {

class DOMStorageContextWrapper;
class DOMStorageSession;

class SessionStorageNamespaceImpl : public SessionStorageNamespace {
 public:
  // Constructs a SessionStorageNamespaceImpl and allocates a new ID for it.
  //
  // The CONTENT_EXPORT allows TestRenderViewHost to instantiate these.
  CONTENT_EXPORT static scoped_refptr<SessionStorageNamespaceImpl> Create(
      scoped_refptr<DOMStorageContextWrapper> context);

  // If there is an existing SessionStorageNamespaceImpl with the given id in
  // the DOMStorageContextWrapper, this will return that object. Otherwise this
  // constructs a SessionStorageNamespaceImpl and assigns |namepace_id| to it.
  static scoped_refptr<SessionStorageNamespaceImpl> Create(
      scoped_refptr<DOMStorageContextWrapper> context,
      const std::string& namepace_id);

  // Constructs a |SessionStorageNamespaceImpl| with id |namespace_id| by
  // cloning |namespace_to_clone|.
  static scoped_refptr<SessionStorageNamespaceImpl> CloneFrom(
      scoped_refptr<DOMStorageContextWrapper> context,
      std::string namepace_id,
      const std::string& namepace_id_to_clone);

  // SessionStorageNamespace implementation.
  const std::string& id() const override;
  void SetShouldPersist(bool should_persist) override;
  bool should_persist() const override;

  SessionStorageNamespaceImpl* Clone();
  bool IsFromContext(DOMStorageContextWrapper* context);

 private:
  explicit SessionStorageNamespaceImpl(
      std::unique_ptr<DOMStorageSession> session);
  ~SessionStorageNamespaceImpl() override;

  std::unique_ptr<DOMStorageSession> session_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageNamespaceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
