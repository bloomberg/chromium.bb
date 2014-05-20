// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_base.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "content/public/browser/web_contents.h"
#include "jni/TabModelBase_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

TabModelBase::TabModelBase(JNIEnv* env, jobject obj, Profile* profile)
    : TabModel(profile),
      java_object_(env, env->NewWeakGlobalRef(obj)) {
}

void TabModelBase::Destroy(JNIEnv* env, jobject obj) {
  TabModelList::RemoveTabModel(this);
  delete this;
}

ScopedJavaLocalRef<jobject> TabModelBase::GetProfileAndroid(JNIEnv* env,
                                                            jobject obj) {
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(GetProfile());
  if (!profile_android)
    return ScopedJavaLocalRef<jobject>();

  return profile_android->GetJavaObject();
}

void TabModelBase::TabAddedToModel(JNIEnv* env, jobject obj, jobject jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);

  // Tab#initialize() should have been called by now otherwise we can't push
  // the window id.
  DCHECK(tab);

  tab->SetWindowSessionID(GetSessionId());
}

int TabModelBase::GetTabCount() const {
  JNIEnv* env = AttachCurrentThread();
  jint count = Java_TabModelBase_getCount(
      env, java_object_.get(env).obj());
  return count;
}

int TabModelBase::GetActiveIndex() const {
  JNIEnv* env = AttachCurrentThread();
  jint index = Java_TabModelBase_index(
      env, java_object_.get(env).obj());
  return index;
}

content::WebContents* TabModelBase::GetWebContentsAt(
    int index) const {
  TabAndroid* tab = GetTabAt(index);
  return tab == NULL ? NULL : tab->web_contents();
}

TabAndroid* TabModelBase::GetTabAt(int index) const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jtab =
      Java_TabModelBase_getTabAt(env,
                                 java_object_.get(env).obj(),
                                 index);
  DCHECK(!jtab.is_null());

  return TabAndroid::GetNativeTab(env, jtab.obj());
}

void TabModelBase::SetActiveIndex(int index) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelBase_setIndex(
      env,
      java_object_.get(env).obj(),
      index);
}

void TabModelBase::CloseTabAt(int index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jtab =
      Java_TabModelBase_getTabAt(env,
                                 java_object_.get(env).obj(),
                                 index);
  if (!jtab.is_null()) {
    Java_TabModelBase_closeTab(env,
                               java_object_.get(env).obj(),
                               jtab.obj());
  }
}

void TabModelBase::CreateTab(content::WebContents* web_contents,
                             int parent_tab_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelBase_createTabWithNativeContents(
      env, java_object_.get(env).obj(),
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      reinterpret_cast<intptr_t>(web_contents), parent_tab_id);
}

content::WebContents* TabModelBase::CreateNewTabForDevTools(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl = ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jobject> obj =
      Java_TabModelBase_createNewTabForDevTools(
          env,
          java_object_.get(env).obj(),
          jurl.obj());
  if (obj.is_null()) {
    VLOG(0) << "Failed to create java tab";
    return NULL;
  }
  TabAndroid* tab = TabAndroid::GetNativeTab(env, obj.obj());
  if (!tab) {
    VLOG(0) << "Failed to create java tab";
    return NULL;
  }
  return tab->web_contents();
}

bool TabModelBase::IsSessionRestoreInProgress() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelBase_isSessionRestoreInProgress(
      env, java_object_.get(env).obj());
}

void TabModelBase::BroadcastSessionRestoreComplete(JNIEnv* env,
                                                   jobject obj) {
  TabModel::BroadcastSessionRestoreComplete();
}

TabModelBase::~TabModelBase() {
}

namespace {

static Profile* FindProfile(jboolean is_incognito) {
  if (g_browser_process == NULL ||
      g_browser_process->profile_manager() == NULL) {
    LOG(ERROR) << "Browser process or profile manager not initialized";
    return NULL;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (is_incognito) {
    return profile->GetOffTheRecordProfile();
  }
  return profile;
}

}  // namespace

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static jlong Init(JNIEnv* env, jobject obj, jboolean is_incognito) {
  Profile* profile = FindProfile(is_incognito);
  TabModel* tab_model = new TabModelBase(env, obj, profile);
  TabModelList::AddTabModel(tab_model);
  return reinterpret_cast<intptr_t>(tab_model);
}

// Register native methods

bool RegisterTabModelBase(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
