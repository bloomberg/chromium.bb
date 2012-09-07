// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"
#include "webkit/glue/resource_type.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceContext;

// Each URLRequest allocated by the ResourceDispatcherHost has a
// ResourceRequestInfo instance associated with it.
class ResourceRequestInfo {
 public:
  // Returns the ResourceRequestInfo associated with the given URLRequest.
  CONTENT_EXPORT static const ResourceRequestInfo* ForRequest(
      const net::URLRequest* request);

  // Allocates a new, dummy ResourceRequestInfo and associates it with the
  // given URLRequest.
  // NOTE: Add more parameters if you need to initialize other fields.
  CONTENT_EXPORT static void AllocateForTesting(
      net::URLRequest* request,
      ResourceType::Type resource_type,
      ResourceContext* context,
      int render_process_id,
      int render_view_id);

  // Returns the associated RenderView for a given process. Returns false, if
  // there is no associated RenderView. This method does not rely on the
  // request being allocated by the ResourceDispatcherHost, but works for all
  // URLRequests that are associated with a RenderView.
  CONTENT_EXPORT static bool GetRenderViewForRequest(
      const net::URLRequest* request,
      int* render_process_id,
      int* render_view_id);

  // Returns the associated ResourceContext.
  virtual ResourceContext* GetContext() const = 0;

  // The child process unique ID of the requestor.
  virtual int GetChildID() const = 0;

  // The IPC route identifier for this request (this identifies the RenderView
  // or like-thing in the renderer that the request gets routed to).
  virtual int GetRouteID() const = 0;

  // The pid of the originating process, if the request is sent on behalf of a
  // another process.  Otherwise it is 0.
  virtual int GetOriginPID() const = 0;

  // Unique identifier (within the scope of the child process) for this request.
  virtual int GetRequestID() const = 0;

  // True if GetFrameID() represents a main frame in the RenderView.
  virtual bool IsMainFrame() const = 0;

  // Frame ID that sent this resource request. -1 if unknown / invalid.
  virtual int64 GetFrameID() const = 0;

  // True if GetParentFrameID() represents a main frame in the RenderView.
  virtual bool ParentIsMainFrame() const = 0;

  // Frame ID of parent frame of frame that sent this resource request.
  // -1 if unknown / invalid.
  virtual int64 GetParentFrameID() const = 0;

  // Returns the associated resource type.
  virtual ResourceType::Type GetResourceType() const = 0;

  // Returns the associated referrer policy.
  virtual WebKit::WebReferrerPolicy GetReferrerPolicy() const = 0;

  // True if the request was initiated by a user action (like a tap to follow
  // a link).
  virtual bool HasUserGesture() const = 0;

  // True if ResourceController::CancelAndIgnore() was called.  For example,
  // the requested URL may be being loaded by an external program.
  virtual bool WasIgnoredByHandler() const = 0;

  // Returns false if there is NOT an associated render view.
  virtual bool GetAssociatedRenderView(int* render_process_id,
                                       int* render_view_id) const = 0;

 protected:
  virtual ~ResourceRequestInfo() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_
