// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_
#define CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_response.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "url/gurl.h"

namespace base {

class SingleThreadTaskRunner;

}  // namespace base

namespace content {

class ResourceDispatcher;
struct ResourceResponseInfo;

// PlzNavigate: Used to override parameters of the navigation request.
struct StreamOverrideParameters {
 public:
  // TODO(clamy): The browser should be made aware on destruction of this struct
  // that it can release its associated stream handle.
  GURL stream_url;
  ResourceResponseHead response;
};

class CONTENT_EXPORT WebURLLoaderImpl
    : public NON_EXPORTED_BASE(blink::WebURLLoader) {
 public:
  explicit WebURLLoaderImpl(
      ResourceDispatcher* resource_dispatcher,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~WebURLLoaderImpl();

  static blink::WebURLError CreateError(const blink::WebURL& unreachable_url,
                                        bool stale_copy_in_cache,
                                        int reason);
  static void PopulateURLResponse(
      const GURL& url,
      const ResourceResponseInfo& info,
      blink::WebURLResponse* response);

  // WebURLLoader methods:
  virtual void loadSynchronously(
      const blink::WebURLRequest& request,
      blink::WebURLResponse& response,
      blink::WebURLError& error,
      blink::WebData& data) override;
  virtual void loadAsynchronously(
      const blink::WebURLRequest& request,
      blink::WebURLLoaderClient* client) override;
  virtual void cancel() override;
  virtual void setDefersLoading(bool value) override;
  virtual void didChangePriority(blink::WebURLRequest::Priority new_priority,
                                 int intra_priority_value) override;
  virtual bool attachThreadedDataReceiver(
      blink::WebThreadedDataReceiver* threaded_data_receiver) override;

 private:
  class Context;
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_
