// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"

class RenderProcessHost;
class RenderFrameHostImpl;
class RenderViewHostImpl;

namespace content {

// When a page's frames are rendered by multiple processes, each renderer has a
// full copy of the frame tree. It has full RenderFrames for the frames it is
// responsible for rendering and placeholder objects (i.e., RenderFrameProxies)
// for frames rendered by other processes.
//
// This class is the browser-side host object for the placeholder. Each node in
// the frame tree has a RenderFrameHost for the active SiteInstance and a set
// of RenderFrameProxyHost objects - one for all other SiteInstances with
// references to this frame. The proxies allow us to keep existing window
// references valid over cross-process navigations and route cross-site
// asynchronous JavaScript calls, such as postMessage.
//
// For now, RenderFrameProxyHost is created when a RenderFrameHost is swapped
// out and acts just as a wrapper. If a RenderFrameHost can be deleted, no
// proxy object is created. It is destroyed when the RenderFrameHost is swapped
// back in or is no longer referenced and is therefore deleted.
//
// Long term, RenderFrameProxyHost will be created whenever a cross-site
// navigation occurs and a reference to the frame navigating needs to be kept
// alive. A RenderFrameProxyHost and a RenderFrameHost for the same SiteInstance
// can exist at the same time, but only one will be "active" at a time.
// There are two cases where the two objects will coexist:
// * When navigating cross-process and there is already a RenderFrameProxyHost
// for the new SiteInstance. A pending RenderFrameHost is created, but it is
// not used until it commits. At that point, RenderFrameHostManager transitions
// the pending RenderFrameHost to the active one and deletes the proxy.
// * When navigating cross-process and the existing document has an unload
// event handler. When the new navigation commits, RenderFrameHostManager
// creates a RenderFrameProxyHost for the old SiteInstance and uses it going
// forward. It also instructs the RenderFrameHost to run the unload event
// handler and is kept alive for the duration. Once the event handling is
// complete, the RenderFrameHost is deleted.
class RenderFrameProxyHost {
 public:
  explicit RenderFrameProxyHost(
       scoped_ptr<RenderFrameHostImpl> render_frame_host);
  ~RenderFrameProxyHost();

  RenderProcessHost* GetProcess() {
    return render_frame_host_->GetProcess();
  }

  SiteInstance* GetSiteInstance() {
    return site_instance_.get();
  }

  // TODO(nasko): The following methods should be removed when swapping out
  // of RenderFrameHosts is no longer used.
  RenderFrameHostImpl* render_frame_host() {
    return render_frame_host_.get();
  }
  RenderViewHostImpl* render_view_host() {
    return render_frame_host_->render_view_host();
  }
  scoped_ptr<RenderFrameHostImpl> PassFrameHost() {
    return render_frame_host_.Pass();
  }

 private:
  scoped_refptr<SiteInstance> site_instance_;

  // TODO(nasko): For now, hide the RenderFrameHost inside the proxy, but remove
  // it once we have all the code support for proper proxy objects.
  scoped_ptr<RenderFrameHostImpl> render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameProxyHost);
};

}  // namespace

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_PROXY_HOST_H_
