// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBFACTORY_IMPL_H_
#define CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBFACTORY_IMPL_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"

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

#endif  // CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBFACTORY_IMPL_H_
