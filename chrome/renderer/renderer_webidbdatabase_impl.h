// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBIDBDATABASE_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBIDBDATABASE_IMPL_H_

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabase.h"

namespace WebKit {
class WebFrame;
class WebIDBCallbacks;
class WebString;
}

class RendererWebIDBDatabaseImpl : public WebKit::WebIDBDatabase {
 public:
  explicit RendererWebIDBDatabaseImpl(int32 idb_database_id);
  virtual ~RendererWebIDBDatabaseImpl();

  // WebKit::WebIDBDatabase
  virtual WebKit::WebString name() const;
  virtual WebKit::WebString description() const;
  virtual WebKit::WebString version() const;
  virtual WebKit::WebDOMStringList objectStores() const;
  virtual void createObjectStore(
      const WebKit::WebString& name, const WebKit::WebString& key_path,
      bool auto_increment, WebKit::WebIDBCallbacks* callbacks);

 private:
  int32 idb_database_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBIDBDATABASE_IMPL_H_
