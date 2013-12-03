// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_

#include <vector>
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/child/npapi/npobject_stub.h"

class RouteIDGenerator;
struct NPObject;

namespace IPC {
class Message;
}

namespace content {
class NPChannelBase;
class RenderViewHost;
struct NPVariant_Param;

// This class handles injecting Java objects into a single RenderView. The Java
// object itself lives in the browser process on a background thread, while a
// proxy object is created in the renderer. An instance of this class exists
// for each RenderViewHost.
class JavaBridgeDispatcherHost
    : public base::RefCountedThreadSafe<JavaBridgeDispatcherHost> {
 public:
  // We hold a weak pointer to the RenderViewhost. It must outlive this object.
  explicit JavaBridgeDispatcherHost(RenderViewHost* render_view_host);

  // Injects |object| into the main frame of the corresponding RenderView. A
  // proxy object is created in the renderer and when the main frame's window
  // object is next cleared, this proxy object is bound to the window object
  // using |name|. The proxy object remains bound until the next time the
  // window object is cleared after a call to RemoveNamedObject() or
  // AddNamedObject() with the same name. The proxy object proxies calls back
  // to |object|, which is manipulated on the background thread. This class
  // holds a reference to |object| for the time that the proxy object is bound
  // to the window object.
  void AddNamedObject(const base::string16& name, NPObject* object);
  void RemoveNamedObject(const base::string16& name);

  // Since this object is ref-counted, it might outlive render_view_host_.
  void RenderViewDeleted();

  void OnGetChannelHandle(IPC::Message* reply_msg);

 private:
  friend class base::RefCountedThreadSafe<JavaBridgeDispatcherHost>;
  virtual ~JavaBridgeDispatcherHost();

  void Send(IPC::Message* msg);

  void GetChannelHandle(IPC::Message* reply_msg);
  void CreateNPVariantParam(NPObject* object, NPVariant_Param* param);
  void CreateObjectStub(NPObject* object, int render_process_id, int route_id);

  scoped_refptr<NPChannelBase> channel_;
  RenderViewHost* render_view_host_;
  std::vector<base::WeakPtr<NPObjectStub> > stubs_;

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_
