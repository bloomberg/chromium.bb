// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"

#include "base/android/java_handler_thread.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/atomic_sequence_num.h"
#include "base/lazy_instance.h"
#include "base/pickle.h"
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
#include "content/public/browser/render_process_host.h"
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
  ~JavaBridgeThread() override { Stop(); }
  static bool CurrentlyOn();
};

base::LazyInstance<JavaBridgeThread> g_background_thread =
    LAZY_INSTANCE_INITIALIZER;

// static
bool JavaBridgeThread::CurrentlyOn() {
  return base::MessageLoop::current() ==
         g_background_thread.Get().message_loop();
}

// Object IDs are globally unique, so we can figure out the right
// GinJavaBridgeDispatcherHost when dispatching messages on the background
// thread.
base::StaticAtomicSequenceNumber g_next_object_id;

}  // namespace

GinJavaBridgeDispatcherHost::GinJavaBridgeDispatcherHost(
    WebContents* web_contents,
    jobject retained_object_set)
    : WebContentsObserver(web_contents),
      BrowserMessageFilter(GinJavaBridgeMsgStart),
      browser_filter_added_(false),
      retained_object_set_(base::android::AttachCurrentThread(),
                           retained_object_set),
      allow_object_contents_inspection_(true),
      current_routing_id_(MSG_ROUTING_NONE) {
  DCHECK(retained_object_set);
}

GinJavaBridgeDispatcherHost::~GinJavaBridgeDispatcherHost() {
}

// GinJavaBridgeDispatcherHost gets created earlier than RenderProcessHost
// is initialized. So we postpone installing the message filter until we know
// that the RPH is in a good shape. Currently this means that we are calling
// this function from any UI thread function that is about to communicate
// with the renderer.
// TODO(mnaganov): Redesign, so we only have a single filter for all hosts.
void GinJavaBridgeDispatcherHost::AddBrowserFilterIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Transient objects can only appear after named objects were added. Thus,
  // we can wait until we have one, to avoid installing unnecessary filters.
  if (!browser_filter_added_ &&
      web_contents()->GetRenderProcessHost()->GetChannel() &&
      !named_objects_.empty()) {
    web_contents()->GetRenderProcessHost()->AddFilter(this);
    browser_filter_added_ = true;
  }
}

void GinJavaBridgeDispatcherHost::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AddBrowserFilterIfNeeded();
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
  AddBrowserFilterIfNeeded();
  base::AutoLock locker(objects_lock_);
  auto iter = objects_.begin();
  while (iter != objects_.end()) {
    JavaObjectWeakGlobalRef ref =
        RemoveHolderAndAdvanceLocked(render_frame_host->GetRoutingID(), &iter);
    if (!ref.is_empty()) {
      RemoveFromRetainedObjectSetLocked(ref);
    }
  }
}

GinJavaBoundObject::ObjectID GinJavaBridgeDispatcherHost::AddObject(
    const base::android::JavaRef<jobject>& object,
    const base::android::JavaRef<jclass>& safe_annotation_clazz,
    bool is_named,
    int32 holder) {
  // Can be called on any thread. Calls come from the UI thread via
  // AddNamedObject, and from the background thread, when injected Java
  // object's method returns a Java object.
  DCHECK(is_named || holder);
  JNIEnv* env = base::android::AttachCurrentThread();
  JavaObjectWeakGlobalRef ref(env, object.obj());
  scoped_refptr<GinJavaBoundObject> new_object =
      is_named ? GinJavaBoundObject::CreateNamed(ref, safe_annotation_clazz)
               : GinJavaBoundObject::CreateTransient(ref, safe_annotation_clazz,
                                                     holder);
  // Note that we are abusing the fact that StaticAtomicSequenceNumber
  // uses Atomic32 as a counter, so it is guaranteed that it will not
  // overflow our int32 IDs. IDs start from 1.
  GinJavaBoundObject::ObjectID object_id = g_next_object_id.GetNext() + 1;
  {
    base::AutoLock locker(objects_lock_);
    objects_[object_id] = new_object;
  }
#if DCHECK_IS_ON()
  {
    GinJavaBoundObject::ObjectID added_object_id;
    DCHECK(FindObjectId(object, &added_object_id));
    DCHECK_EQ(object_id, added_object_id);
  }
#endif  // DCHECK_IS_ON()
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
        retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    base::AutoLock locker(objects_lock_);
    JNI_Java_HashSet_add(env, retained_object_set, object);
  }
  return object_id;
}

bool GinJavaBridgeDispatcherHost::FindObjectId(
    const base::android::JavaRef<jobject>& object,
    GinJavaBoundObject::ObjectID* object_id) {
  // Can be called on any thread.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::AutoLock locker(objects_lock_);
  for (const auto& pair : objects_) {
    if (env->IsSameObject(
            object.obj(),
            pair.second->GetLocalRef(env).obj())) {
      *object_id = pair.first;
      return true;
    }
  }
  return false;
}

JavaObjectWeakGlobalRef GinJavaBridgeDispatcherHost::GetObjectWeakRef(
    GinJavaBoundObject::ObjectID object_id) {
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get())
    return object->GetWeakRef();
  else
    return JavaObjectWeakGlobalRef();
}

JavaObjectWeakGlobalRef
GinJavaBridgeDispatcherHost::RemoveHolderAndAdvanceLocked(
    int32 holder,
    ObjectMap::iterator* iter_ptr) {
  objects_lock_.AssertAcquired();
  JavaObjectWeakGlobalRef result;
  scoped_refptr<GinJavaBoundObject> object((*iter_ptr)->second);
  if (!object->IsNamed()) {
    object->RemoveHolder(holder);
    if (!object->HasHolders()) {
      result = object->GetWeakRef();
      objects_.erase((*iter_ptr)++);
    }
  } else {
    ++(*iter_ptr);
  }
  return result;
}

void GinJavaBridgeDispatcherHost::RemoveFromRetainedObjectSetLocked(
    const JavaObjectWeakGlobalRef& ref) {
  objects_lock_.AssertAcquired();
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_remove(env, retained_object_set, ref.get(env));
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
    base::AutoLock locker(objects_lock_);
    objects_[object_id]->AddName();
  } else {
    object_id = AddObject(object, safe_annotation_clazz, true, 0);
  }
  named_objects_[name] = object_id;

  AddBrowserFilterIfNeeded();
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

  {
    base::AutoLock locker(objects_lock_);
    objects_[iter->second]->RemoveName();
  }
  named_objects_.erase(iter);

  // As the object isn't going to be removed from the JavaScript side until the
  // next page reload, calls to it must still work, thus we should continue to
  // hold it. All the transient objects and removed named objects will be purged
  // during the cleansing caused by DocumentAvailableInMainFrame event.

  web_contents()->SendToAllFrames(
      new GinJavaBridgeMsg_RemoveNamedObject(MSG_ROUTING_NONE, copied_name));
}

void GinJavaBridgeDispatcherHost::SetAllowObjectContentsInspection(bool allow) {
  if (!JavaBridgeThread::CurrentlyOn()) {
    g_background_thread.Get().message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(
            &GinJavaBridgeDispatcherHost::SetAllowObjectContentsInspection,
            this, allow));
    return;
  }
  allow_object_contents_inspection_ = allow;
}

void GinJavaBridgeDispatcherHost::DocumentAvailableInMainFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Called when the window object has been cleared in the main frame.
  // That means, all sub-frames have also been cleared, so only named
  // objects survived.
  AddBrowserFilterIfNeeded();
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  base::AutoLock locker(objects_lock_);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_clear(env, retained_object_set);
  }
  auto iter = objects_.begin();
  while (iter != objects_.end()) {
    if (iter->second->IsNamed()) {
      if (!retained_object_set.is_null()) {
        JNI_Java_HashSet_add(
            env, retained_object_set, iter->second->GetLocalRef(env));
      }
      ++iter;
    } else {
      objects_.erase(iter++);
    }
  }
}

base::TaskRunner* GinJavaBridgeDispatcherHost::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  GinJavaBoundObject::ObjectID object_id = 0;
  // TODO(mnaganov): It's very sad that we have a BrowserMessageFilter per
  // WebView instance. We should redesign to have a filter per RPH.
  // Check, if the object ID in the message is known to this host. If not,
  // this is a message for some other host. As all our IPC messages from the
  // renderer start with object ID, we just fetch it directly from the
  // message, considering sync and async messages separately.
  switch (message.type()) {
    case GinJavaBridgeHostMsg_GetMethods::ID:
    case GinJavaBridgeHostMsg_HasMethod::ID:
    case GinJavaBridgeHostMsg_InvokeMethod::ID: {
      DCHECK(message.is_sync());
      PickleIterator message_reader =
          IPC::SyncMessage::GetDataIterator(&message);
      if (!IPC::ReadParam(&message, &message_reader, &object_id))
        return NULL;
      break;
    }
    case GinJavaBridgeHostMsg_ObjectWrapperDeleted::ID: {
      DCHECK(!message.is_sync());
      PickleIterator message_reader(message);
      if (!IPC::ReadParam(&message, &message_reader, &object_id))
        return NULL;
      break;
    }
    default:
      NOTREACHED();
      return NULL;
  }
  {
    base::AutoLock locker(objects_lock_);
    if (objects_.find(object_id) != objects_.end()) {
        return g_background_thread.Get().message_loop()->task_runner().get();
    }
  }
  return NULL;
}

bool GinJavaBridgeDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  // We can get here As WebContentsObserver also has OnMessageReceived,
  // or because we have not provided a task runner in
  // OverrideTaskRunnerForMessage. In either case, just bail out.
  if (!JavaBridgeThread::CurrentlyOn())
    return false;
  SetCurrentRoutingID(message.routing_id());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GinJavaBridgeDispatcherHost, message)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_GetMethods, OnGetMethods)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_HasMethod, OnHasMethod)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_InvokeMethod, OnInvokeMethod)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_ObjectWrapperDeleted,
                        OnObjectWrapperDeleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  SetCurrentRoutingID(MSG_ROUTING_NONE);
  return handled;
}

void GinJavaBridgeDispatcherHost::OnDestruct() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
  } else {
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }
}

int GinJavaBridgeDispatcherHost::GetCurrentRoutingID() const {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  return current_routing_id_;
}

void GinJavaBridgeDispatcherHost::SetCurrentRoutingID(int32 routing_id) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  current_routing_id_ = routing_id;
}

scoped_refptr<GinJavaBoundObject> GinJavaBridgeDispatcherHost::FindObject(
    GinJavaBoundObject::ObjectID object_id) {
  // Can be called on any thread.
  base::AutoLock locker(objects_lock_);
  auto iter = objects_.find(object_id);
  if (iter != objects_.end())
    return iter->second;
  return NULL;
}

void GinJavaBridgeDispatcherHost::OnGetMethods(
    GinJavaBoundObject::ObjectID object_id,
    std::set<std::string>* returned_method_names) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  if (!allow_object_contents_inspection_)
    return;
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get()) {
    *returned_method_names = object->GetMethodNames();
  } else {
    LOG(ERROR) << "WebView: Unknown object: " << object_id;
  }
}

void GinJavaBridgeDispatcherHost::OnHasMethod(
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    bool* result) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (object.get()) {
    *result = object->HasMethod(method_name);
  } else {
    LOG(ERROR) << "WebView: Unknown object: " << object_id;
  }
}

void GinJavaBridgeDispatcherHost::OnInvokeMethod(
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    base::ListValue* wrapped_result,
    content::GinJavaBridgeError* error_code) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  DCHECK(GetCurrentRoutingID() != MSG_ROUTING_NONE);
  scoped_refptr<GinJavaBoundObject> object = FindObject(object_id);
  if (!object.get()) {
    LOG(ERROR) << "WebView: Unknown object: " << object_id;
    wrapped_result->Append(base::Value::CreateNullValue());
    *error_code = kGinJavaBridgeUnknownObjectId;
    return;
  }
  scoped_refptr<GinJavaMethodInvocationHelper> result =
      new GinJavaMethodInvocationHelper(
          make_scoped_ptr(new GinJavaBoundObjectDelegate(object)),
          method_name,
          arguments);
  result->Init(this);
  result->Invoke();
  *error_code = result->GetInvocationError();
  if (result->HoldsPrimitiveResult()) {
    scoped_ptr<base::ListValue> result_copy(
        result->GetPrimitiveResult().DeepCopy());
    wrapped_result->Swap(result_copy.get());
  } else if (!result->GetObjectResult().is_null()) {
    GinJavaBoundObject::ObjectID returned_object_id;
    if (FindObjectId(result->GetObjectResult(), &returned_object_id)) {
      base::AutoLock locker(objects_lock_);
      objects_[returned_object_id]->AddHolder(GetCurrentRoutingID());
    } else {
      returned_object_id = AddObject(result->GetObjectResult(),
                                     result->GetSafeAnnotationClass(),
                                     false,
                                     GetCurrentRoutingID());
    }
    wrapped_result->Append(
        GinJavaBridgeValue::CreateObjectIDValue(
            returned_object_id).release());
  } else {
    wrapped_result->Append(base::Value::CreateNullValue());
  }
}

void GinJavaBridgeDispatcherHost::OnObjectWrapperDeleted(
    GinJavaBoundObject::ObjectID object_id) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  DCHECK(GetCurrentRoutingID() != MSG_ROUTING_NONE);
  base::AutoLock locker(objects_lock_);
  auto iter = objects_.find(object_id);
  if (iter == objects_.end())
    return;
  JavaObjectWeakGlobalRef ref =
      RemoveHolderAndAdvanceLocked(GetCurrentRoutingID(), &iter);
  if (!ref.is_empty()) {
    RemoveFromRetainedObjectSetLocked(ref);
  }
}

}  // namespace content
