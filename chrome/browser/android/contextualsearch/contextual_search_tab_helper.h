// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_TAB_HELPER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

class ContextualSearchTabHelper {
 public:
  ContextualSearchTabHelper(JNIEnv* env, jobject obj, Profile* profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  ~ContextualSearchTabHelper();
  void OnContextualSearchPrefChanged();

  JavaObjectWeakGlobalRef weak_java_ref_;
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::WeakPtrFactory<ContextualSearchTabHelper> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ContextualSearchTabHelper);
};

bool RegisterContextualSearchTabHelper(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_TAB_HELPER_H_
