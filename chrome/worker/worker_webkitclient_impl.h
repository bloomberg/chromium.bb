// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_WORKER_WORKER_WEBKIT_CLIENT_IMPL_H_
#define CHROME_WORKER_WORKER_WEBKIT_CLIENT_IMPL_H_

#include "webkit/api/public/WebMimeRegistry.h"
#include "webkit/glue/webkitclient_impl.h"

class WorkerWebKitClientImpl : public webkit_glue::WebKitClientImpl,
                               public WebKit::WebMimeRegistry {
 public:
  // WebKitClient methods:
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel();
  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& policy_url,
                          const WebKit::WebString& value);
  virtual WebKit::WebString cookies(const WebKit::WebURL& url,
                                    const WebKit::WebURL& policy_url);
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual bool getFileSize(const WebKit::WebString& path, long long& result);
  virtual WebKit::WebString defaultLocale();
  virtual WebKit::WebSharedWorkerRepository* sharedWorkerRepository();

  // WebMimeRegistry methods:
  virtual WebKit::WebMimeRegistry::SupportsType supportsMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsImageMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsJavaScriptMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const WebKit::WebString&, const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsNonImageMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeForExtension(const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeFromFile(const WebKit::WebString&);
  virtual WebKit::WebString preferredExtensionForMIMEType(
      const WebKit::WebString&);
};

#endif  // CHROME_WORKER_WORKER_WEBKIT_CLIENT_IMPL_H_
