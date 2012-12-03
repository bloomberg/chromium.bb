// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_

#include <vector>
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "content/common/npobject_stub.h"
#include "content/public/browser/render_view_host_observer.h"

class RouteIDGenerator;
struct NPObject;

namespace content {
class NPChannelBase;
class RenderViewHost;
struct NPVariant_Param;

// This class handles injecting Java objects into a single RenderView. The Java
// object itself lives in the browser process on a background thread, while a
// proxy object is created in the renderer. An instance of this class exists
// for each RenderViewHost.
class JavaBridgeDispatcherHost
    : public base::RefCountedThreadSafe<JavaBridgeDispatcherHost>,
      public RenderViewHostObserver {
 public:
  // We hold a weak pointer to the RenderViewhost. It must outlive this object.
  JavaBridgeDispatcherHost(RenderViewHost* render_view_host);

  // Injects |object| into the main frame of the corresponding RenderView. A
  // proxy object is created in the renderer and when the main frame's window
  // object is next cleared, this proxy object is bound to the window object
  // using |name|. The proxy object remains bound until the next time the
  // window object is cleared after a call to RemoveNamedObject() or
  // AddNamedObject() with the same name. The proxy object proxies calls back
  // to |object|, which is manipulated on the background thread. This class
  // holds a reference to |object| for the time that the proxy object is bound
  // to the window object.
  void AddNamedObject(const string16& name, NPObject* object);
  void RemoveNamedObject(const string16& name);

  // RenderViewHostObserver overrides:
  // The IPC macros require this to be public.
  virtual bool Send(IPC::Message* msg) OVERRIDE;
  virtual void RenderViewHostDestroyed(
      RenderViewHost* render_view_host) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<JavaBridgeDispatcherHost>;
  virtual ~JavaBridgeDispatcherHost();

  // RenderViewHostObserver override:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message handlers
  void OnGetChannelHandle(IPC::Message* reply_msg);

  void GetChannelHandle(IPC::Message* reply_msg);
  void CreateNPVariantParam(NPObject* object, NPVariant_Param* param);
  void CreateObjectStub(NPObject* object, int route_id);

  scoped_refptr<NPChannelBase> channel_;
  bool is_renderer_initialized_;
  std::vector<base::WeakPtr<NPObjectStub> > stubs_;

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_
