// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_THROTTLING_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_THROTTLING_RESOURCE_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/renderer_host/layered_resource_handler.h"
#include "content/public/browser/resource_controller.h"
#include "googleurl/src/gurl.h"

namespace content {

class ResourceThrottle;
struct ResourceResponse;

// Used to apply a list of ResourceThrottle instances to an URLRequest.
class ThrottlingResourceHandler : public LayeredResourceHandler,
                                  public ResourceController {
 public:
  // Takes ownership of the ResourceThrottle instances.
  ThrottlingResourceHandler(scoped_ptr<ResourceHandler> next_handler,
                            int child_id,
                            int request_id,
                            ScopedVector<ResourceThrottle> throttles);
  virtual ~ThrottlingResourceHandler();

  // LayeredResourceHandler overrides:
  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 content::ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillStart(int request_id, const GURL& url,
                           bool* defer) OVERRIDE;

  // ResourceThrottleController implementation:
  virtual void Cancel() OVERRIDE;
  virtual void CancelAndIgnore() OVERRIDE;
  virtual void Resume() OVERRIDE;

 private:
  void ResumeStart();
  void ResumeRedirect();
  void ResumeResponse();

  enum DeferredStage {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_RESPONSE
  };
  DeferredStage deferred_stage_;

  int request_id_;

  ScopedVector<ResourceThrottle> throttles_;
  size_t index_;

  GURL deferred_url_;
  scoped_refptr<ResourceResponse> deferred_response_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_THROTTLING_RESOURCE_HANDLER_H_
