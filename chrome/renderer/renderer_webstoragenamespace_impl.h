// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBSTORAGENAMESPACE_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBSTORAGENAMESPACE_IMPL_H_

#include "base/basictypes.h"
#include "webkit/api/public/WebStorageNamespace.h"

class RendererWebStorageNamespaceImpl : public WebKit::WebStorageNamespace {
 public:
  explicit RendererWebStorageNamespaceImpl(bool is_local_storage);
  RendererWebStorageNamespaceImpl(bool is_local_storage, int64 namespace_id);

  // See WebStorageNamespace.h for documentation on these functions.
  virtual ~RendererWebStorageNamespaceImpl();
  virtual WebKit::WebStorageArea* createStorageArea(
      const WebKit::WebString& origin);
  virtual WebKit::WebStorageNamespace* copy();
  virtual void close();

 private:
  // Are we local storage (as opposed to session storage).  Used during lazy
  // initialization of namespace_id_.
  bool is_local_storage_;

  // Our namespace ID.  Lazily initialized.
  int64 namespace_id_;

  // namespace_id_ should equal this iff its unitialized.
  static const int64 kUninitializedNamespaceId = -1;
};

#endif  // CHROME_RENDERER_WEBSTORAGENAMESPACE_IMPL_H_
