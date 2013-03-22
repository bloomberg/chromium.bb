// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_BROWSER_WEBKITPLATFORMSUPPORT_IMPL_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_BROWSER_WEBKITPLATFORMSUPPORT_IMPL_H_

#include "content/common/webkitplatformsupport_impl.h"
#include "webkit/glue/webfileutilities_impl.h"

namespace content {

class BrowserWebKitPlatformSupportImpl : public WebKitPlatformSupportImpl {
 public:
  BrowserWebKitPlatformSupportImpl();
  virtual ~BrowserWebKitPlatformSupportImpl();

  // WebKitPlatformSupport methods:
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
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long long availableDiskSpaceInBytes(
      const WebKit::WebString& fileName);

 private:
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_BROWSER_WEBKITPLATFORMSUPPORT_IMPL_H_
