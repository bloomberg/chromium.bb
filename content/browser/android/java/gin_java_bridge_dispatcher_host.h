// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_

#include <map>
#include <set>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/android/java/gin_java_bound_object.h"
#include "content/browser/android/java/gin_java_method_invocation_helper.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class ListValue;
}

namespace IPC {
class Message;
}

namespace content {

// This class handles injecting Java objects into a single RenderView. The Java
// object itself lives in the browser process on a background thread, while a
// proxy object is created in the renderer. An instance of this class exists
// for each RenderFrameHost.
class GinJavaBridgeDispatcherHost
    : public base::SupportsWeakPtr<GinJavaBridgeDispatcherHost>,
      public WebContentsObserver,
      public GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:

  GinJavaBridgeDispatcherHost(WebContents* web_contents,
                              jobject retained_object_set);
  virtual ~GinJavaBridgeDispatcherHost();

  void AddNamedObject(
      const std::string& name,
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);
  void RemoveNamedObject(const std::string& name);
  void SetAllowObjectContentsInspection(bool allow);

  // WebContentsObserver
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void DocumentAvailableInMainFrame() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 RenderFrameHost* render_frame_host) OVERRIDE;

  // GinJavaMethodInvocationHelper::DispatcherDelegate
  virtual JavaObjectWeakGlobalRef GetObjectWeakRef(
      GinJavaBoundObject::ObjectID object_id) OVERRIDE;

  void OnGetMethods(RenderFrameHost* render_frame_host,
                    GinJavaBoundObject::ObjectID object_id,
                    IPC::Message* reply_msg);
  void OnHasMethod(RenderFrameHost* render_frame_host,
                   GinJavaBoundObject::ObjectID object_id,
                   const std::string& method_name,
                   IPC::Message* reply_msg);
  void OnInvokeMethod(RenderFrameHost* render_frame_host,
                      GinJavaBoundObject::ObjectID object_id,
                      const std::string& method_name,
                      const base::ListValue& arguments,
                      IPC::Message* reply_msg);

 private:
  void OnObjectWrapperDeleted(RenderFrameHost* render_frame_host,
                              GinJavaBoundObject::ObjectID object_id);

  bool IsValidRenderFrameHost(RenderFrameHost* render_frame_host);
  void SendMethods(RenderFrameHost* render_frame_host,
                   const std::set<std::string>& method_names);
  void SendHasMethodReply(RenderFrameHost* render_frame_host,
                          bool result);
  void ProcessMethodInvocationResult(
      RenderFrameHost* render_frame_host,
      scoped_refptr<GinJavaMethodInvocationHelper> result);
  void ProcessMethodInvocationObjectResult(
      RenderFrameHost* render_frame_host,
      scoped_refptr<GinJavaMethodInvocationHelper> result);
  GinJavaBoundObject::ObjectID AddObject(
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      bool is_named,
      RenderFrameHost* holder);
  bool FindObjectId(const base::android::JavaRef<jobject>& object,
                    GinJavaBoundObject::ObjectID* object_id);
  void RemoveHolder(RenderFrameHost* holder,
                    const GinJavaBoundObject::ObjectMap::iterator& from,
                    size_t count);
  bool HasPendingReply(RenderFrameHost* render_frame_host) const;
  IPC::Message* TakePendingReply(RenderFrameHost* render_frame_host);

  // Every time a GinJavaBoundObject backed by a real Java object is
  // created/destroyed, we insert/remove a strong ref to that Java object into
  // this set so that it doesn't get garbage collected while it's still
  // potentially in use. Although the set is managed native side, it's owned
  // and defined in Java so that pushing refs into it does not create new GC
  // roots that would prevent ContentViewCore from being garbage collected.
  JavaObjectWeakGlobalRef retained_object_set_;
  bool allow_object_contents_inspection_;
  GinJavaBoundObject::ObjectMap objects_;
  typedef std::map<std::string, GinJavaBoundObject::ObjectID> NamedObjectMap;
  NamedObjectMap named_objects_;

  // Keep track of pending calls out to Java such that we can send a synchronous
  // reply to the renderer waiting on the response should the RenderFrame be
  // destroyed while the reply is pending.
  // Only used on the UI thread.
  typedef std::map<RenderFrameHost*, IPC::Message*> PendingReplyMap;
  PendingReplyMap pending_replies_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaBridgeDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_
