// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_BROWSER_WEBKITCLIENT_IMPL_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_BROWSER_WEBKITCLIENT_IMPL_H_
#pragma once

#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkitclient_impl.h"

class IndexedDBKeyUtilityClient;

class BrowserWebKitClientImpl : public webkit_glue::WebKitClientImpl {
 public:
  BrowserWebKitClientImpl();
  virtual ~BrowserWebKitClientImpl();

  // WebKitClient methods:
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebFileUtilities* fileUtilities();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel();
  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& first_party_for_cookies,
                          const WebKit::WebString& value);
  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url,
      const WebKit::WebURL& first_party_for_cookies);
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual WebKit::WebString defaultLocale();
  virtual WebKit::WebThemeEngine* themeEngine();
  virtual WebKit::WebURLLoader* createURLLoader();
  virtual WebKit::WebSocketStreamHandle* createSocketStreamHandle();
  virtual void getPluginList(bool refresh, WebKit::WebPluginListBuilder*);
  virtual WebKit::WebData loadResource(const char* name);
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota);
  virtual void dispatchStorageEvent(const WebKit::WebString& key,
      const WebKit::WebString& oldValue, const WebKit::WebString& newValue,
      const WebKit::WebString& origin, const WebKit::WebURL& url,
      bool isLocalStorage);
  virtual WebKit::WebSharedWorkerRepository* sharedWorkerRepository();
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual void createIDBKeysFromSerializedValuesAndKeyPath(
      const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values,
      const WebKit::WebString& keyPath,
      WebKit::WebVector<WebKit::WebIDBKey>& keys);

 private:
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_BROWSER_WEBKITCLIENT_IMPL_H_
