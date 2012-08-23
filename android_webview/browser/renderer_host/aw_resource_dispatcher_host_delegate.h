// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_RENDERER_HOST_AW_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define ANDROID_WEBVIEW_LIB_RENDERER_HOST_AW_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include "content/public/browser/resource_dispatcher_host_delegate.h"

#include "base/lazy_instance.h"

namespace android_webview {

class AwResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  static void ResourceDispatcherHostCreated();

  // Overriden methods from ResourceDispatcherHostDelegate.
  virtual void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      ResourceType::Type resource_type,
      int child_id,
      int route_id,
      bool is_continuation_of_transferred_request,
      ScopedVector<content::ResourceThrottle>* throttles);

 private:
  friend struct base::DefaultLazyInstanceTraits<
      AwResourceDispatcherHostDelegate>;
  AwResourceDispatcherHostDelegate();
  virtual ~AwResourceDispatcherHostDelegate();

  DISALLOW_COPY_AND_ASSIGN(AwResourceDispatcherHostDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_RENDERER_HOST_AW_RESOURCE_DISPATCHER_HOST_H_
