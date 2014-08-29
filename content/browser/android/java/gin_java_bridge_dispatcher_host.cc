// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"

#include "base/android/java_handler_thread.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "content/browser/android/java/gin_java_bound_object_delegate.h"
#include "content/browser/android/java/jni_helper.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "content/common/android/hash_set.h"
#include "content/common/gin_java_bridge_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_utils.h"

#if !defined(OS_ANDROID)
#error "JavaBridge only supports OS_ANDROID"
#endif

namespace content {

namespace {
// The JavaBridge needs to use a Java thread so the callback
// will happen on a thread with a prepared Looper.
class JavaBridgeThread : public base::android::JavaHandlerThread {
 public:
  JavaBridgeThread() : base::android::JavaHandlerThread("JavaBridge") {
    Start();
  }
  virtual ~JavaBridgeThread() {
    Stop();
  }
};

base::LazyInstance<JavaBridgeThread> g_background_thread =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

GinJavaBridgeDispatcherHost::GinJavaBridgeDispatcherHost(
    WebContents* web_contents,
    jobject retained_object_set)
    : WebContentsObserver(web_contents),
      retained_object_set_(base::android::AttachCurrentThread(),
                           retained_object_set),
      allow_object_contents_inspection_(true) {
  DCHECK(retained_object_set);
}

GinJavaBridgeDispatcherHost::~GinJavaBridgeDispatcherHost() {
  DCHECK(pending_replies_.empty());
}

void GinJavaBridgeDispatcherHost::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  for (NamedObjectMap::const_iterator iter = named_objects_.begin();
       iter != named_objects_.end();
       ++iter) {
    render_frame_host->Send(new GinJavaBridgeMsg_AddNamedObject(
        render_frame_host->GetRoutingID(), iter->first, iter->second));
  }
}

void GinJavaBridgeDispatcherHost::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  IPC::Message* reply_msg = TakePendingReply(render_frame_host);
  if (reply_msg != NULL) {
    base::ListValue result;
    result.Append(base::Value::CreateNullValue());
    IPC::WriteParam(reply_msg, result);
    IPC::WriteParam(reply_msg, kGinJavaBridgeRenderFrameDeleted);
    render_frame_host->Send(reply_msg);
  }
  RemoveHolder(render_frame_host,
               GinJavaBoundObject::ObjectMap::iterator(&objects_),
               objects_.size());
}

GinJavaBoundObject::ObjectID GinJavaBridgeDispatcherHost::AddObject(
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    bool is_named,
    RenderFrameHost* holder) {
  DCHECK(is_named || holder);
  GinJavaBoundObject::ObjectID object_id;
  JNIEnv* env = base::android::AttachCurrentThread();
  JavaObjectWeakGlobalRef ref(env, object.obj());
  if (is_named) {
    object_id = objects_.Add(new scoped_refptr<GinJavaBoundObject>(
        GinJavaBoundObject::CreateNamed(ref, safe_annotation_clazz)));
  } else {
    object_id = objects_.Add(new scoped_refptr<GinJavaBoundObject>(
        GinJavaBoundObject::CreateTransient(
            ref, safe_annotation_clazz, holder)));
  }
#if DCHECK_IS_ON
  {
    GinJavaBoundObject::ObjectID added_object_id;
    DCHECK(FindObjectId(object, &added_object_id));
    DCHECK_EQ(object_id, added_object_id);
  }
#endif  // DCHECK_IS_ON
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
        retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_add(env, retained_object_set, object);
  }
  return object_id;
}

bool GinJavaBridgeDispatcherHost::FindObjectId(
    const base::android::JavaRef<jobject>& object,
    GinJavaBoundObject::ObjectID* object_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  for (GinJavaBoundObject::ObjectMap::iterator it(&objects_); !it.IsAtEnd();
       it.Advance()) {
    if (env->IsSameObject(
            object.obj(),
            it.GetCurrentValue()->get()->GetLocalRef(env).obj())) {
      *object_id = it.GetCurrentKey();
      return true;
    }
  }
  return false;
}

JavaObjectWeakGlobalRef GinJavaBridgeDispatcherHost::GetObjectWeakRef(
    GinJavaBoundObject::ObjectID object_id) {
  scoped_refptr<GinJavaBoundObject>* result = objects_.Lookup(object_id);
  scoped_refptr<GinJavaBoundObject> object(result ? *result : NULL);
  if (object.get())
    return object->GetWeakRef();
  else
    return JavaObjectWeakGlobalRef();
}

void GinJavaBridgeDispatcherHost::RemoveHolder(
    RenderFrameHost* holder,
    const GinJavaBoundObject::ObjectMap::iterator& from,
    size_t count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  size_t i = 0;
  for (GinJavaBoundObject::ObjectMap::iterator it(from);
       !it.IsAtEnd() && i < count;
       it.Advance(), ++i) {
    scoped_refptr<GinJavaBoundObject> object(*it.GetCurrentValue());
    if (object->IsNamed())
      continue;
    object->RemoveHolder(holder);
    if (!object->HasHolders()) {
      if (!retained_object_set.is_null()) {
        JNI_Java_HashSet_remove(
            env, retained_object_set, object->GetLocalRef(env));
      }
      objects_.Remove(it.GetCurrentKey());
    }
  }
}

void GinJavaBridgeDispatcherHost::AddNamedObject(
    const std::string& name,
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GinJavaBoundObject::ObjectID object_id;
  NamedObjectMap::iterator iter = named_objects_.find(name);
  bool existing_object = FindObjectId(object, &object_id);
  if (existing_object && iter != named_objects_.end() &&
      iter->second == object_id) {
    // Nothing to do.
    return;
  }
  if (iter != named_objects_.end()) {
    RemoveNamedObject(iter->first);
  }
  if (existing_object) {
    (*objects_.Lookup(object_id))->AddName();
  } else {
    object_id = AddObject(object, safe_annotation_clazz, true, NULL);
  }
  named_objects_[name] = object_id;

  web_contents()->SendToAllFrames(
      new GinJavaBridgeMsg_AddNamedObject(MSG_ROUTING_NONE, name, object_id));
}

void GinJavaBridgeDispatcherHost::RemoveNamedObject(
    const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NamedObjectMap::iterator iter = named_objects_.find(name);
  if (iter == named_objects_.end())
    return;

  // |name| may come from |named_objects_|. Make a copy of name so that if
  // |name| is from |named_objects_| it'll be valid after the remove below.
  const std::string copied_name(name);

  scoped_refptr<GinJavaBoundObject> object(*objects_.Lookup(iter->second));
  named_objects_.erase(iter);
  object->RemoveName();

  // Not erasing from the objects map, as we can still receive method
  // invocation requests for this object, and they should work until the
  // java object is gone.
  if (!object->IsNamed()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> retained_object_set =
        retained_object_set_.get(env);
    if (!retained_object_set.is_null()) {
      JNI_Java_HashSet_remove(
          env, retained_object_set, object->GetLocalRef(env));
    }
  }

  web_contents()->SendToAllFrames(
      new GinJavaBridgeMsg_RemoveNamedObject(MSG_ROUTING_NONE, copied_name));
}

void GinJavaBridgeDispatcherHost::SetAllowObjectContentsInspection(bool allow) {
  allow_object_contents_inspection_ = allow;
}

void GinJavaBridgeDispatcherHost::DocumentAvailableInMainFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Called when the window object has been cleared in the main frame.
  // That means, all sub-frames have also been cleared, so only named
  // objects survived.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_clear(env, retained_object_set);
  }

  // We also need to add back the named objects we have so far as they
  // should survive navigations.
  for (GinJavaBoundObject::ObjectMap::iterator it(&objects_); !it.IsAtEnd();
       it.Advance()) {
    scoped_refptr<GinJavaBoundObject> object(*it.GetCurrentValue());
    if (object->IsNamed()) {
      if (!retained_object_set.is_null()) {
        JNI_Java_HashSet_add(
            env, retained_object_set, object->GetLocalRef(env));
      }
    } else {
      objects_.Remove(it.GetCurrentKey());
    }
  }
}

namespace {

// TODO(mnaganov): Implement passing of a parameter into sync message handlers.
class MessageForwarder : public IPC::Sender {
 public:
  MessageForwarder(GinJavaBridgeDispatcherHost* gjbdh,
                   RenderFrameHost* render_frame_host)
      : gjbdh_(gjbdh), render_frame_host_(render_frame_host) {}
  void OnGetMethods(GinJavaBoundObject::ObjectID object_id,
                    IPC::Message* reply_msg) {
    gjbdh_->OnGetMethods(render_frame_host_,
                         object_id,
                         reply_msg);
  }
  void OnHasMethod(GinJavaBoundObject::ObjectID object_id,
                   const std::string& method_name,
                   IPC::Message* reply_msg) {
    gjbdh_->OnHasMethod(render_frame_host_,
                        object_id,
                        method_name,
                        reply_msg);
  }
  void OnInvokeMethod(GinJavaBoundObject::ObjectID object_id,
                      const std::string& method_name,
                      const base::ListValue& arguments,
                      IPC::Message* reply_msg) {
    gjbdh_->OnInvokeMethod(render_frame_host_,
                           object_id,
                           method_name,
                           arguments,
                           reply_msg);
  }
  virtual bool Send(IPC::Message* msg) OVERRIDE {
    NOTREACHED();
    return false;
  }
 private:
  GinJavaBridgeDispatcherHost* gjbdh_;
  RenderFrameHost* render_frame_host_;
};

}

bool GinJavaBridgeDispatcherHost::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  bool handled = true;
  MessageForwarder forwarder(this, render_frame_host);
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(GinJavaBridgeDispatcherHost, message,
                                   render_frame_host)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(GinJavaBridgeHostMsg_GetMethods,
                                    &forwarder,
                                    MessageForwarder::OnGetMethods)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(GinJavaBridgeHostMsg_HasMethod,
                                    &forwarder,
                                    MessageForwarder::OnHasMethod)
    IPC_MESSAGE_FORWARD_DELAY_REPLY(GinJavaBridgeHostMsg_InvokeMethod,
                                    &forwarder,
                                    MessageForwarder::OnInvokeMethod)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_ObjectWrapperDeleted,
                        OnObjectWrapperDeleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

namespace {

class IsValidRenderFrameHostHelper
    : public base::RefCounted<IsValidRenderFrameHostHelper> {
 public:
  explicit IsValidRenderFrameHostHelper(RenderFrameHost* rfh_to_match)
      : rfh_to_match_(rfh_to_match), rfh_found_(false) {}

  bool rfh_found() { return rfh_found_; }

  void OnFrame(RenderFrameHost* rfh) {
    if (rfh_to_match_ == rfh) rfh_found_ = true;
  }

 private:
  friend class base::RefCounted<IsValidRenderFrameHostHelper>;

  ~IsValidRenderFrameHostHelper() {}

  RenderFrameHost* rfh_to_match_;
  bool rfh_found_;

  DISALLOW_COPY_AND_ASSIGN(IsValidRenderFrameHostHelper);
};

}  // namespace

bool GinJavaBridgeDispatcherHost::IsValidRenderFrameHost(
    RenderFrameHost* render_frame_host) {
  scoped_refptr<IsValidRenderFrameHostHelper> helper =
      new IsValidRenderFrameHostHelper(render_frame_host);
  web_contents()->ForEachFrame(
      base::Bind(&IsValidRenderFrameHostHelper::OnFrame, helper));
  return helper->rfh_found();
}

void GinJavaBridgeDispatcherHost::OnGetMethods(
    RenderFrameHost* render_frame_host,
    GinJavaBoundObject::ObjectID object_id,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);
  if (!allow_object_contents_inspection_) {
    IPC::WriteParam(reply_msg, std::set<std::string>());
    render_frame_host->Send(reply_msg);
    return;
  }
  scoped_refptr<GinJavaBoundObject> object(*objects_.Lookup(object_id));
  if (!object) {
    LOG(ERROR) << "WebView: Unknown object: " << object_id;
    IPC::WriteParam(reply_msg, std::set<std::string>());
    render_frame_host->Send(reply_msg);
    return;
  }
  DCHECK(!HasPendingReply(render_frame_host));
  pending_replies_[render_frame_host] = reply_msg;
  base::PostTaskAndReplyWithResult(
      g_background_thread.Get().message_loop()->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&GinJavaBoundObject::GetMethodNames, object),
      base::Bind(&GinJavaBridgeDispatcherHost::SendMethods,
                 AsWeakPtr(),
                 render_frame_host));
}

void GinJavaBridgeDispatcherHost::SendMethods(
    RenderFrameHost* render_frame_host,
    const std::set<std::string>& method_names) {
  IPC::Message* reply_msg = TakePendingReply(render_frame_host);
  if (!reply_msg) {
    return;
  }
  IPC::WriteParam(reply_msg, method_names);
  render_frame_host->Send(reply_msg);
}

void GinJavaBridgeDispatcherHost::OnHasMethod(
    RenderFrameHost* render_frame_host,
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);
  scoped_refptr<GinJavaBoundObject> object(*objects_.Lookup(object_id));
  if (!object) {
    LOG(ERROR) << "WebView: Unknown object: " << object_id;
    IPC::WriteParam(reply_msg, false);
    render_frame_host->Send(reply_msg);
    return;
  }
  DCHECK(!HasPendingReply(render_frame_host));
  pending_replies_[render_frame_host] = reply_msg;
  base::PostTaskAndReplyWithResult(
      g_background_thread.Get().message_loop()->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&GinJavaBoundObject::HasMethod, object, method_name),
      base::Bind(&GinJavaBridgeDispatcherHost::SendHasMethodReply,
                 AsWeakPtr(),
                 render_frame_host));
}

void GinJavaBridgeDispatcherHost::SendHasMethodReply(
    RenderFrameHost* render_frame_host,
    bool result) {
  IPC::Message* reply_msg = TakePendingReply(render_frame_host);
  if (!reply_msg) {
    return;
  }
  IPC::WriteParam(reply_msg, result);
  render_frame_host->Send(reply_msg);
}

void GinJavaBridgeDispatcherHost::OnInvokeMethod(
    RenderFrameHost* render_frame_host,
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);
  scoped_refptr<GinJavaBoundObject> object(*objects_.Lookup(object_id));
  if (!object) {
    LOG(ERROR) << "WebView: Unknown object: " << object_id;
    base::ListValue result;
    result.Append(base::Value::CreateNullValue());
    IPC::WriteParam(reply_msg, result);
    IPC::WriteParam(reply_msg, kGinJavaBridgeUnknownObjectId);
    render_frame_host->Send(reply_msg);
    return;
  }
  DCHECK(!HasPendingReply(render_frame_host));
  pending_replies_[render_frame_host] = reply_msg;
  scoped_refptr<GinJavaMethodInvocationHelper> result =
      new GinJavaMethodInvocationHelper(
          make_scoped_ptr(new GinJavaBoundObjectDelegate(object))
              .PassAs<GinJavaMethodInvocationHelper::ObjectDelegate>(),
          method_name,
          arguments);
  result->Init(this);
  g_background_thread.Get()
      .message_loop()
      ->message_loop_proxy()
      ->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&GinJavaMethodInvocationHelper::Invoke, result),
          base::Bind(
              &GinJavaBridgeDispatcherHost::ProcessMethodInvocationResult,
              AsWeakPtr(),
              render_frame_host,
              result));
}

void GinJavaBridgeDispatcherHost::ProcessMethodInvocationResult(
    RenderFrameHost* render_frame_host,
    scoped_refptr<GinJavaMethodInvocationHelper> result) {
  if (result->HoldsPrimitiveResult()) {
    IPC::Message* reply_msg = TakePendingReply(render_frame_host);
    if (!reply_msg) {
      return;
    }
    IPC::WriteParam(reply_msg, result->GetPrimitiveResult());
    IPC::WriteParam(reply_msg, result->GetInvocationError());
    render_frame_host->Send(reply_msg);
  } else {
    ProcessMethodInvocationObjectResult(render_frame_host, result);
  }
}

void GinJavaBridgeDispatcherHost::ProcessMethodInvocationObjectResult(
    RenderFrameHost* render_frame_host,
    scoped_refptr<GinJavaMethodInvocationHelper> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsValidRenderFrameHost(render_frame_host)) {
    // In this case, we must've already sent the reply when the render frame
    // was destroyed.
    DCHECK(!HasPendingReply(render_frame_host));
    return;
  }

  base::ListValue wrapped_result;
  if (!result->GetObjectResult().is_null()) {
    GinJavaBoundObject::ObjectID returned_object_id;
    if (FindObjectId(result->GetObjectResult(), &returned_object_id)) {
      (*objects_.Lookup(returned_object_id))->AddHolder(render_frame_host);
    } else {
      returned_object_id = AddObject(result->GetObjectResult(),
                                     result->GetSafeAnnotationClass(),
                                     false,
                                     render_frame_host);
    }
    wrapped_result.Append(
        GinJavaBridgeValue::CreateObjectIDValue(
            returned_object_id).release());
  } else {
    wrapped_result.Append(base::Value::CreateNullValue());
  }
  IPC::Message* reply_msg = TakePendingReply(render_frame_host);
  if (!reply_msg) {
    return;
  }
  IPC::WriteParam(reply_msg, wrapped_result);
  IPC::WriteParam(reply_msg, result->GetInvocationError());
  render_frame_host->Send(reply_msg);
}

void GinJavaBridgeDispatcherHost::OnObjectWrapperDeleted(
    RenderFrameHost* render_frame_host,
    GinJavaBoundObject::ObjectID object_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);
  if (objects_.Lookup(object_id)) {
    GinJavaBoundObject::ObjectMap::iterator iter(&objects_);
    while (!iter.IsAtEnd() && iter.GetCurrentKey() != object_id)
      iter.Advance();
    DCHECK(!iter.IsAtEnd());
    RemoveHolder(render_frame_host, iter, 1);
  }
}

IPC::Message* GinJavaBridgeDispatcherHost::TakePendingReply(
    RenderFrameHost* render_frame_host) {
  if (!IsValidRenderFrameHost(render_frame_host)) {
    DCHECK(!HasPendingReply(render_frame_host));
    return NULL;
  }

  PendingReplyMap::iterator it = pending_replies_.find(render_frame_host);
  // There may be no pending reply if we're called from RenderFrameDeleted and
  // we already sent the reply through the regular route.
  if (it == pending_replies_.end()) {
    return NULL;
  }

  IPC::Message* reply_msg = it->second;
  pending_replies_.erase(it);
  return reply_msg;
}

bool GinJavaBridgeDispatcherHost::HasPendingReply(
    RenderFrameHost* render_frame_host) const {
  return pending_replies_.find(render_frame_host) != pending_replies_.end();
}

}  // namespace content
