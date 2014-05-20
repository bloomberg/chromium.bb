// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_BASE_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_BASE_H_

#include <jni.h>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"

class Profile;
class TabAndroid;

namespace content {
class WebContents;
}

// Native representation of TabModelBase which provides access to information
// about a tabstrip to native code and could potentially be used in place of
// Browser for some functionality in Clank.
class TabModelBase : public TabModel {
 public:
  TabModelBase(JNIEnv* env, jobject obj, Profile* profile);
  void Destroy(JNIEnv* env, jobject obj);

  // Called by JNI
  base::android::ScopedJavaLocalRef<jobject> GetProfileAndroid(JNIEnv* env,
                                                               jobject obj);
  void TabAddedToModel(JNIEnv* env, jobject obj, jobject jtab);

  // TabModel:
  virtual int GetTabCount() const OVERRIDE;
  virtual int GetActiveIndex() const OVERRIDE;
  virtual content::WebContents* GetWebContentsAt(int index) const OVERRIDE;
  virtual TabAndroid* GetTabAt(int index) const OVERRIDE;

  virtual void SetActiveIndex(int index) OVERRIDE;
  virtual void CloseTabAt(int index) OVERRIDE;

  virtual void CreateTab(content::WebContents* web_contents,
                         int parent_tab_id) OVERRIDE;
  virtual content::WebContents* CreateNewTabForDevTools(
      const GURL& url) OVERRIDE;

  // Return true if we are currently restoring sessions asynchronously.
  virtual bool IsSessionRestoreInProgress() const OVERRIDE;

  // Instructs the TabModel to broadcast a notification that all tabs are now
  // loaded from storage.
  void BroadcastSessionRestoreComplete(JNIEnv* env, jobject obj);

 private:
  virtual ~TabModelBase();

  JavaObjectWeakGlobalRef java_object_;
};

// Register the Tab's native methods through jni.
bool RegisterTabModelBase(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_BASE_H_
