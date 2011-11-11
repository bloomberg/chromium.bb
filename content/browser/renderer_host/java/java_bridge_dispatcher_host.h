// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/string16.h"

class JavaBridgeChannelHost;
class RenderViewHost;
struct NPObject;

// This class handles injecting Java objects into a single RenderView. The Java
// object itself lives on the browser's WEBKIT thread, while a proxy object is
// created in the renderer. An instance of this class exists for each
// RenderViewHost.
class JavaBridgeDispatcherHost :
    public base::RefCountedThreadSafe<JavaBridgeDispatcherHost> {
 public:
  // We hold a weak pointer to the RenderViewhost. It must outlive this object.
  JavaBridgeDispatcherHost(RenderViewHost* render_view_host);

  // Injects |object| into the main frame of the corresponding RenderView. A
  // proxy object is created in the renderer and when the main frame's window
  // object is next cleared, this proxy object is bound to the window object
  // using |name|. The proxy object remains bound until the next time the
  // window object is cleared after a call to RemoveNamedObject() or
  // AddNamedObject() with the same name. The proxy object proxies calls back
  // to |object|, which is manipulated on the WEBKIT thread. This class holds a
  // reference to |object| for the time that the proxy object is bound to the
  // window object.
  void AddNamedObject(const string16& name, NPObject* object);
  void RemoveNamedObject(const string16& name);

 private:
  friend class base::RefCountedThreadSafe<JavaBridgeDispatcherHost>;
  ~JavaBridgeDispatcherHost();

  void DoAddNamedObject(const string16& name, NPObject* object);
  void EnsureChannelIsSetUp();

  RenderViewHost* render_view_host_;
  scoped_refptr<JavaBridgeChannelHost> channel_;

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeDispatcherHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_H_
