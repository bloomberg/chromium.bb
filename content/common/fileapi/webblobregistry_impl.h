// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FILEAPI_WEBBLOBREGISTRY_IMPL_H_
#define CONTENT_COMMON_FILEAPI_WEBBLOBREGISTRY_IMPL_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebBlobRegistry.h"

namespace WebKit {
class WebBlobData;
class WebURL;
}

namespace content {
class ChildThread;

class WebBlobRegistryImpl : public WebKit::WebBlobRegistry {
 public:
  explicit WebBlobRegistryImpl(ChildThread* child_thread);
  virtual ~WebBlobRegistryImpl();

  // See WebBlobRegistry.h for documentation on these functions.
  virtual void registerBlobURL(const WebKit::WebURL& url,
                               WebKit::WebBlobData& data);
  virtual void registerBlobURL(const WebKit::WebURL& url,
                               const WebKit::WebURL& src_url);
  virtual void unregisterBlobURL(const WebKit::WebURL& url);

 private:
  ChildThread* child_thread_;
};

}  // namespace content

#endif  // CONTENT_COMMON_FILEAPI_WEBBLOBREGISTRY_IMPL_H_
