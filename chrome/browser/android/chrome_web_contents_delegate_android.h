// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include <jni.h>

#include "base/files/file_path.h"
#include "base/time.h"
#include "components/web_contents_delegate_android/web_contents_delegate_android.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class FindNotificationDetails;

namespace content {
struct FileChooserParams;
class WebContents;
}

namespace gfx {
class Rect;
class RectF;
}

namespace chrome {
namespace android {

// Chromium Android specific WebContentsDelegate.
// Should contain any WebContentsDelegate implementations required by
// the Chromium Android port but not to be shared with WebView.
class ChromeWebContentsDelegateAndroid
    : public components::WebContentsDelegateAndroid,
      public content::NotificationObserver {
 public:
  ChromeWebContentsDelegateAndroid(JNIEnv* env, jobject obj);
  virtual ~ChromeWebContentsDelegateAndroid();

  virtual void RunFileChooser(content::WebContents* web_contents,
                              const content::FileChooserParams& params)
                              OVERRIDE;
  virtual void CloseContents(content::WebContents* web_contents) OVERRIDE;
  virtual void FindReply(content::WebContents* web_contents,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual void FindMatchRectsReply(content::WebContents* web_contents,
                                   int version,
                                   const std::vector<gfx::RectF>& rects,
                                   const gfx::RectF& active_rect) OVERRIDE;
  virtual content::JavaScriptDialogManager*
  GetJavaScriptDialogManager() OVERRIDE;
  virtual bool CanDownload(content::RenderViewHost* source,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual void OnStartDownload(content::WebContents* source,
                               content::DownloadItem* download) OVERRIDE;
  virtual void DidNavigateToPendingEntry(content::WebContents* source) OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(
      content::WebContents* source) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual bool RequestPpapiBrokerPermission(
      content::WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      const base::Callback<void(bool)>& callback) OVERRIDE;

 private:
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnFindResultAvailable(content::WebContents* web_contents,
                             const FindNotificationDetails* find_result);

  content::NotificationRegistrar notification_registrar_;

  base::TimeTicks navigation_start_time_;
};

// Register the native methods through JNI.
bool RegisterChromeWebContentsDelegateAndroid(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_WEB_CONTENTS_DELEGATE_ANDROID_H_
