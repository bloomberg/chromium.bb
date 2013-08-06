// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FILEAPI_WEBBLOBREGISTRY_IMPL_H_
#define CONTENT_CHILD_FILEAPI_WEBBLOBREGISTRY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "webkit/common/blob/blob_data.h"

namespace WebKit {
class WebBlobData;
class WebString;
class WebThreadSafeData;
class WebURL;
}

namespace content {
class ThreadSafeSender;

class WebBlobRegistryImpl : public WebKit::WebBlobRegistry {
 public:
  explicit WebBlobRegistryImpl(ThreadSafeSender* sender);
  virtual ~WebBlobRegistryImpl();

  virtual void registerBlobURL(const WebKit::WebURL& url,
                               WebKit::WebBlobData& data);
  virtual void registerBlobURL(const WebKit::WebURL& url,
                               const WebKit::WebURL& src_url);
  virtual void unregisterBlobURL(const WebKit::WebURL& url);

  virtual void registerStreamURL(const WebKit::WebURL& url,
                                 const WebKit::WebString& content_type);
  virtual void registerStreamURL(const WebKit::WebURL& url,
                                 const WebKit::WebURL& src_url);
  virtual void addDataToStream(const WebKit::WebURL& url,
                               WebKit::WebThreadSafeData& data);
  virtual void finalizeStream(const WebKit::WebURL& url);
  virtual void unregisterStreamURL(const WebKit::WebURL& url);

 private:
  void SendDataForBlob(const WebKit::WebURL& url,
                       const WebKit::WebThreadSafeData& data);

  scoped_refptr<ThreadSafeSender> sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_FILEAPI_WEBBLOBREGISTRY_IMPL_H_
