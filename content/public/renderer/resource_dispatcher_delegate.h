// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RESOURCE_DISPATCHER_DELEGATE_H_
#define CONTENT_PUBLIC_RENDERER_RESOURCE_DISPATCHER_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"

namespace content {

class RequestPeer;
struct ResourceResponseHead;

// Interface that allows observing request events and optionally replacing
// the peer. Note that if it doesn't replace the peer it must return the
// current peer so that the ownership is continued to be held by
// ResourceDispatcher.
class CONTENT_EXPORT ResourceDispatcherDelegate {
 public:
  virtual ~ResourceDispatcherDelegate() {}

  virtual std::unique_ptr<RequestPeer> OnRequestComplete(
      std::unique_ptr<RequestPeer> current_peer,
      ResourceType resource_type,
      int error_code) = 0;

  // Note that |url|, |referrer| and |method| are the final values (e.g. after
  // any redirects).
  virtual std::unique_ptr<RequestPeer> OnReceivedResponse(
      std::unique_ptr<RequestPeer> current_peer,
      int render_frame_id,
      const GURL& url,
      const GURL& referrer,
      const std::string& method,
      ResourceType resource_type,
      const ResourceResponseHead& response_head) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RESOURCE_DISPATCHER_DELEGATE_H_
