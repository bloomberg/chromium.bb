// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBIDBDATABASE_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBIDBDATABASE_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"

namespace WebKit {
class WebFrame;
class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;
class WebString;
class WebIDBTransaction;
}

class RendererWebIDBDatabaseImpl : public WebKit::WebIDBDatabase {
 public:
  explicit RendererWebIDBDatabaseImpl(int32 idb_database_id);
  virtual ~RendererWebIDBDatabaseImpl();

  // WebKit::WebIDBDatabase
  virtual WebKit::WebString name() const;
  virtual WebKit::WebString version() const;
  virtual WebKit::WebDOMStringList objectStoreNames() const;
  virtual WebKit::WebIDBObjectStore* createObjectStore(
      const WebKit::WebString& name,
      const WebKit::WebString& key_path,
      bool auto_increment,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode& ec);
  virtual void deleteObjectStore(
      const WebKit::WebString& name,
      const WebKit::WebIDBTransaction& transaction,
      WebKit::WebExceptionCode& ec);
  virtual void setVersion(
      const WebKit::WebString& version, WebKit::WebIDBCallbacks* callbacks,
      WebKit::WebExceptionCode& ec);
  virtual WebKit::WebIDBTransaction* transaction(
      const WebKit::WebDOMStringList& names,
      unsigned short mode, unsigned long timeout,
      WebKit::WebExceptionCode& ec);
  virtual void close();
  virtual void open(WebKit::WebIDBDatabaseCallbacks*);

 private:
  int32 idb_database_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBIDBDATABASE_IMPL_H_
