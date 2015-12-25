// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class pairs with DownloadController on Java side to forward requests
// for GET downloads to the current DownloadListener. POST downloads are
// handled on the native side.
//
// Both classes are Singleton classes. C++ object owns Java object.
//
// Call sequence
// GET downloads:
// DownloadControllerAndroid::CreateGETDownload() =>
// DownloadController.newHttpGetDownload() =>
// DownloadListener.onDownloadStart() /
// DownloadListener2.requestHttpGetDownload()
//

#ifndef CONTENT_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_IMPL_H_
#define CONTENT_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_IMPL_H_

#include <string>

#include <stdint.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/download_item.h"
#include "net/cookies/cookie_monster.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {
struct GlobalRequestID;
class DeferredDownloadObserver;
class RenderViewHost;
class WebContents;

class DownloadControllerAndroidImpl : public DownloadControllerAndroid,
                                      public DownloadItem::Observer {
 public:
  static DownloadControllerAndroidImpl* GetInstance();

  static bool RegisterDownloadController(JNIEnv* env);

  // Called when DownloadController Java object is instantiated.
  void Init(JNIEnv* env, jobject obj);

  // Removes a deferred download from |deferred_downloads_|.
  void CancelDeferredDownload(DeferredDownloadObserver* observer);

  // DownloadControllerAndroid implementation.
  void AcquireFileAccessPermission(
      WebContents* web_contents,
      const AcquireFileAccessPermissionCallback& callback) override;

 private:
  // Used to store all the information about an Android download.
  struct DownloadInfoAndroid {
    explicit DownloadInfoAndroid(net::URLRequest* request);
    ~DownloadInfoAndroid();

    // The URL from which we are downloading. This is the final URL after any
    // redirection by the server for |original_url_|.
    GURL url;
    // The original URL before any redirection by the server for this URL.
    GURL original_url;
    int64_t total_bytes;
    std::string content_disposition;
    std::string original_mime_type;
    std::string user_agent;
    std::string cookie;
    std::string referer;
    bool has_user_gesture;

    WebContents* web_contents;
    // Default copy constructor is used for passing this struct by value.
  };
  struct JavaObject;
  friend struct base::DefaultSingletonTraits<DownloadControllerAndroidImpl>;
  DownloadControllerAndroidImpl();
  ~DownloadControllerAndroidImpl() override;

  // Helper method for implementing AcquireFileAccessPermission().
  bool HasFileAccessPermission(
      base::android::ScopedJavaLocalRef<jobject> j_content_view_core);

  // DownloadControllerAndroid implementation.
  void CreateGETDownload(int render_process_id,
                         int render_view_id,
                         int request_id) override;
  void OnDownloadStarted(DownloadItem* download_item) override;
  void StartContextMenuDownload(const ContextMenuParams& params,
                                WebContents* web_contents,
                                bool is_link,
                                const std::string& extra_headers) override;
  void DangerousDownloadValidated(WebContents* web_contents,
                                  int download_id,
                                  bool accept) override;

  // DownloadItem::Observer interface.
  void OnDownloadUpdated(DownloadItem* item) override;

  typedef base::Callback<void(const DownloadInfoAndroid&)>
      GetDownloadInfoCB;
  void PrepareDownloadInfo(const GlobalRequestID& global_id,
                           const GetDownloadInfoCB& callback);
  void CheckPolicyAndLoadCookies(const DownloadInfoAndroid& info,
                                 const GetDownloadInfoCB& callback,
                                 const GlobalRequestID& global_id,
                                 const net::CookieList& cookie_list);
  void DoLoadCookies(const DownloadInfoAndroid& info,
                     const GetDownloadInfoCB& callback,
                     const GlobalRequestID& global_id);
  void OnCookieResponse(DownloadInfoAndroid info,
                        const GetDownloadInfoCB& callback,
                        const std::string& cookie);
  void StartDownloadOnUIThread(const GetDownloadInfoCB& callback,
                               const DownloadInfoAndroid& info);
  void StartAndroidDownload(int render_process_id,
                            int render_view_id,
                            const DownloadInfoAndroid& info);
  void StartAndroidDownloadInternal(int render_process_id,
                                    int render_view_id,
                                    const DownloadInfoAndroid& info,
                                    bool allowed);

  // The download item contains dangerous file types.
  void OnDangerousDownload(DownloadItem *item);

  base::android::ScopedJavaLocalRef<jobject> GetContentViewCoreFromWebContents(
      WebContents* web_contents);

  // Creates Java object if it is not created already and returns it.
  JavaObject* GetJavaObject();

  JavaObject* java_object_;

  ScopedVector<DeferredDownloadObserver> deferred_downloads_;

  DISALLOW_COPY_AND_ASSIGN(DownloadControllerAndroidImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_ANDROID_IMPL_H_
