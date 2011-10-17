// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WEBBLOBREGISTRY_IMPL_H_
#define CHROME_COMMON_WEBBLOBREGISTRY_IMPL_H_
#pragma once

//#include "ipc/ipc_message.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlobRegistry.h"

class ChildThread;

namespace base {
class SharedMemory;
}

namespace WebKit {
class WebBlobData;
class WebURL;
}

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

#endif  // CHROME_COMMON_WEBBLOBREGISTRY_IMPL_H_
