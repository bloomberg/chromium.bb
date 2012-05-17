// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "content/public/browser/web_contents_observer.h"

class JavaBridgeDispatcherHost;
struct NPObject;

namespace content {
class RenderViewHost;
}

// This class handles injecting Java objects into all of the RenderViews
// associated with a WebContents. It manages a set of JavaBridgeDispatcherHost
// objects, one per RenderViewHost.
class JavaBridgeDispatcherHostManager : public content::WebContentsObserver {
 public:
  explicit JavaBridgeDispatcherHostManager(content::WebContents* web_contents);
  virtual ~JavaBridgeDispatcherHostManager();

  // These methods add or remove the object to each JavaBridgeDispatcherHost.
  // Each one holds a reference to the NPObject while the object is bound to
  // the corresponding RenderView. See JavaBridgeDispatcherHost for details.
  void AddNamedObject(const string16& name, NPObject* object);
  void RemoveNamedObject(const string16& name);

  // content::WebContentsObserver overrides
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

 private:
  typedef std::map<content::RenderViewHost*,
      scoped_refptr<JavaBridgeDispatcherHost> >
      InstanceMap;
  InstanceMap instances_;
  typedef std::map<string16, NPObject*> ObjectMap;
  ObjectMap objects_;

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeDispatcherHostManager);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
