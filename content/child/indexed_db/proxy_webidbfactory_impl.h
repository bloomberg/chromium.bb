// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_PROXY_WEBIDBFACTORY_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_PROXY_WEBIDBFACTORY_IMPL_H_

#include "third_party/WebKit/public/platform/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/WebIDBFactory.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace WebKit {
class WebString;
}

namespace content {

class RendererWebIDBFactoryImpl : public WebKit::WebIDBFactory {
 public:
  RendererWebIDBFactoryImpl();
  virtual ~RendererWebIDBFactoryImpl();

  // See WebIDBFactory.h for documentation on these functions.
  virtual void getDatabaseNames(
      WebKit::WebIDBCallbacks* callbacks,
      const WebKit::WebString& database_identifier,
      const WebKit::WebString& data_dir);
  virtual void open(
      const WebKit::WebString& name,
      long long version,
      long long transaction_id,
      WebKit::WebIDBCallbacks* callbacks,
      WebKit::WebIDBDatabaseCallbacks* databaseCallbacks,
      const WebKit::WebString& database_identifier,
      const WebKit::WebString& data_dir);
  virtual void deleteDatabase(
      const WebKit::WebString& name,
      WebKit::WebIDBCallbacks* callbacks,
      const WebKit::WebString& database_identifier,
      const WebKit::WebString& data_dir);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_PROXY_WEBIDBFACTORY_IMPL_H_
