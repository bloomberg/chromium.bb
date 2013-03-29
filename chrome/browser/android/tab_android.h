// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_H_

#include <jni.h>

#include "base/android/jni_helper.h"
#include "base/callback_forward.h"
#include "base/string16.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"

class GURL;
class SkBitmap;

namespace browser_sync {
class SyncedTabDelegate;
}

namespace content {
struct ContextMenuParams;
class WebContents;
}

class TabAndroid {
 public:
  TabAndroid(JNIEnv* env, jobject obj);

  // Convenience method to retrieve the Tab associated with the passed
  // WebContents.  Can return NULL.
  static TabAndroid* FromWebContents(content::WebContents* web_contents);

  static TabAndroid* GetNativeTab(JNIEnv* env, jobject obj);

  // TODO(tedchoc): Make pure virtual once all derived classes can be updated.
  virtual content::WebContents* GetWebContents();

  virtual browser_sync::SyncedTabDelegate* GetSyncedTabDelegate() = 0;

  virtual ToolbarModel::SecurityLevel GetSecurityLevel();

  int id() const {
    return tab_id_;
  }

  virtual void OnReceivedHttpAuthRequest(jobject auth_handler,
                                         const string16& host,
                                         const string16& realm) = 0;

  // Called to show the regular context menu that is triggered by a long press.
  virtual void ShowContextMenu(const content::ContextMenuParams& params) = 0;

  // Called to show a custom context menu. Used by the NTP.
  virtual void ShowCustomContextMenu(
      const content::ContextMenuParams& params,
      const base::Callback<void(int)>& callback) = 0;

  // Called when context menu option to create the bookmark shortcut on
  // homescreen is called.
  virtual void AddShortcutToBookmark(
      const GURL& url, const string16& title, const SkBitmap& skbitmap,
      int r_value, int g_value, int b_value) = 0;

  // Called when a bookmark node should be edited.
  virtual void EditBookmark(int64 node_id, bool is_folder) = 0;

  // Called when the common ExternalProtocolHandler wants to
  // run the external protocol dialog.
  // TODO(jknotten): Remove this method. Making it non-abstract, so that
  // derived classes may remove their implementation first.
  virtual void RunExternalProtocolDialog(const GURL& url);

  static bool RegisterTabAndroid(JNIEnv* env);
 protected:
  virtual ~TabAndroid();

  static void InitTabHelpers(content::WebContents* web_contents);

  static content::WebContents* InitWebContentsFromView(JNIEnv* env,
                                                       jobject content_view);

  int tab_id_;

 private:
  JavaObjectWeakGlobalRef weak_java_tab_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
