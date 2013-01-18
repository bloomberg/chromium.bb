// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_

#include <map>

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "content/public/browser/web_contents_observer.h"

struct NPObject;

namespace content {
class JavaBridgeDispatcherHost;
class RenderViewHost;

// This class handles injecting Java objects into all of the RenderViews
// associated with a WebContents. It manages a set of JavaBridgeDispatcherHost
// objects, one per RenderViewHost.
class JavaBridgeDispatcherHostManager
    : public WebContentsObserver,
      public base::SupportsWeakPtr<JavaBridgeDispatcherHostManager> {
 public:
  explicit JavaBridgeDispatcherHostManager(WebContents* web_contents);
  virtual ~JavaBridgeDispatcherHostManager();

  // These methods add or remove the object to each JavaBridgeDispatcherHost.
  // Each one holds a reference to the NPObject while the object is bound to
  // the corresponding RenderView. See JavaBridgeDispatcherHost for details.
  void AddNamedObject(const string16& name, NPObject* object);
  void RemoveNamedObject(const string16& name);

  // Every time a JavaBoundObject backed by a real Java object is
  // created/destroyed, we insert/remove a strong ref to that Java object into
  // this set so that it doesn't get garbage collected while it's still
  // potentially in use. Although the set is managed native side, it's owned
  // and defined in Java so that pushing refs into it does not create new GC
  // roots that would prevent ContentViewCore from being garbage collected.
  void SetRetainedObjectSet(const JavaObjectWeakGlobalRef& retained_object_set);

  // WebContentsObserver overrides
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;
  virtual void DocumentAvailableInMainFrame() OVERRIDE;

  void JavaBoundObjectCreated(const base::android::JavaRef<jobject>& object);
  void JavaBoundObjectDestroyed(const base::android::JavaRef<jobject>& object);

 private:
  typedef std::map<RenderViewHost*, scoped_refptr<JavaBridgeDispatcherHost> >
      InstanceMap;
  InstanceMap instances_;
  typedef std::map<string16, NPObject*> ObjectMap;
  ObjectMap objects_;
  JavaObjectWeakGlobalRef retained_object_set_;

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeDispatcherHostManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_DISPATCHER_HOST_MANAGER_H_
