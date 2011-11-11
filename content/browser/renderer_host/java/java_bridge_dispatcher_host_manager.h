// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
#pragma once

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class JavaBridgeDispatcherHost;
class RenderViewHost;
struct NPObject;

// This class handles injecting Java objects into all of the RenderViews
// associated with a TabContents. It manages a set of JavaBridgeDispatcherHost
// objects, one per RenderViewHost.
class JavaBridgeDispatcherHostManager : public TabContentsObserver {
 public:
  JavaBridgeDispatcherHostManager(TabContents* tab_contents);
  virtual ~JavaBridgeDispatcherHostManager();

  // These methods add or remove the object to each JavaBridgeDispatcherHost.
  // Each one holds a reference to the NPObject while the object is bound to
  // the corresponding RenderView. See JavaBridgeDispatcherHost for details.
  void AddNamedObject(const string16& name, NPObject* object);
  void RemoveNamedObject(const string16& name);

  // TabContentsObserver overrides
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE;
  virtual void TabContentsDestroyed(TabContents* tab_contents) OVERRIDE;

 private:
  typedef std::map<RenderViewHost*, scoped_refptr<JavaBridgeDispatcherHost> >
      InstanceMap;
  InstanceMap instances_;
  typedef std::map<string16, NPObject*> ObjectMap;
  ObjectMap objects_;

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeDispatcherHostManager);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
