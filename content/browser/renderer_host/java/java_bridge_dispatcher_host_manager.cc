// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/browser/renderer_host/java/java_bound_object.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host.h"
#include "content/common/android/hash_set.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"

namespace content {

JavaBridgeDispatcherHostManager::JavaBridgeDispatcherHostManager(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

JavaBridgeDispatcherHostManager::~JavaBridgeDispatcherHostManager() {
  for (ObjectMap::iterator iter = objects_.begin(); iter != objects_.end();
      ++iter) {
    WebKit::WebBindings::releaseObject(iter->second);
  }
  DCHECK_EQ(0U, instances_.size());
}

void JavaBridgeDispatcherHostManager::AddNamedObject(const string16& name,
                                                     NPObject* object) {
  // Record this object in a map so that we can add it into RenderViewHosts
  // created later. The JavaBridgeDispatcherHost instances will take a
  // reference to the object, but we take one too, because this method can be
  // called before there are any such instances.
  WebKit::WebBindings::retainObject(object);
  objects_[name] = object;

  for (InstanceMap::iterator iter = instances_.begin();
      iter != instances_.end(); ++iter) {
    iter->second->AddNamedObject(name, object);
  }
}

void JavaBridgeDispatcherHostManager::SetRetainedObjectSet(
    const JavaObjectWeakGlobalRef& retained_object_set) {
  // It's an error to replace the retained_object_set_ after it's been set,
  // so we check that it hasn't already been here.
  // TODO(benm): It'd be better to pass the set in the constructor to avoid
  // the chance of this happening; but that's tricky as this get's constructed
  // before ContentViewCore (which owns the set). Best solution may be to move
  // ownership of the JavaBridgerDispatchHostManager from WebContents to
  // ContentViewCore?
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> new_retained_object_set =
      retained_object_set.get(env);
  base::android::ScopedJavaLocalRef<jobject> current_retained_object_set =
      retained_object_set_.get(env);
  if (!env->IsSameObject(new_retained_object_set.obj(),
                         current_retained_object_set.obj())) {
    DCHECK(current_retained_object_set.is_null());
    retained_object_set_ = retained_object_set;
  }
}

void JavaBridgeDispatcherHostManager::RemoveNamedObject(const string16& name) {
  ObjectMap::iterator iter = objects_.find(name);
  if (iter == objects_.end()) {
    return;
  }

  WebKit::WebBindings::releaseObject(iter->second);
  objects_.erase(iter);

  for (InstanceMap::iterator iter = instances_.begin();
      iter != instances_.end(); ++iter) {
    iter->second->RemoveNamedObject(name);
  }
}

void JavaBridgeDispatcherHostManager::RenderViewCreated(
    RenderViewHost* render_view_host) {
  // Creates a JavaBridgeDispatcherHost for the specified RenderViewHost and
  // adds all currently registered named objects to the new instance.
  scoped_refptr<JavaBridgeDispatcherHost> instance =
      new JavaBridgeDispatcherHost(render_view_host);

  for (ObjectMap::const_iterator iter = objects_.begin();
      iter != objects_.end(); ++iter) {
    instance->AddNamedObject(iter->first, iter->second);
  }

  instances_[render_view_host] = instance;
}

void JavaBridgeDispatcherHostManager::RenderViewDeleted(
    RenderViewHost* render_view_host) {
  instances_.erase(render_view_host);
}

void JavaBridgeDispatcherHostManager::WebContentsDestroyed(
    WebContents* web_contents) {
  // When a WebContents is shutting down, it clears its observers before
  // it kills all of its RenderViewHosts, so we won't get a call to
  // RenderViewDeleted() for all RenderViewHosts.
  instances_.clear();
}

void JavaBridgeDispatcherHostManager::DocumentAvailableInMainFrame() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Called when the window object has been cleared in the main frame.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_clear(env, retained_object_set);

    // We also need to add back the named objects we have so far as they
    // should survive navigations.
    ObjectMap::iterator it = objects_.begin();
    for (; it != objects_.end(); ++it) {
      JNI_Java_HashSet_add(env, retained_object_set,
                           JavaBoundObject::GetJavaObject(it->second));
    }
  }
}

void JavaBridgeDispatcherHostManager::JavaBoundObjectCreated(
    const base::android::JavaRef<jobject>& object) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_add(env, retained_object_set, object);
  }
}

void JavaBridgeDispatcherHostManager::JavaBoundObjectDestroyed(
    const base::android::JavaRef<jobject>& object) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> retained_object_set =
      retained_object_set_.get(env);
  if (!retained_object_set.is_null()) {
    JNI_Java_HashSet_remove(env, retained_object_set, object);
  }
}

}  // namespace content
