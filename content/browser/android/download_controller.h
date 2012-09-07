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
// DownloadController::NewGetDownload() =>
// DownloadController.newHttpGetDownload() =>
// DownloadListener.onDownloadStart() /
// DownloadListener2.requestHttpGetDownload()
//

#ifndef CONTENT_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_H_
#define CONTENT_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_H_

#include <string>

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "content/public/browser/download_item.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/cookie_monster.h"

namespace net {
class URLRequest;
}

namespace content {
struct GlobalRequestID;
class RenderViewHost;
class WebContents;

class DownloadController : public DownloadItem::Observer {
 public:
  static bool RegisterDownloadController(JNIEnv* env);
  static DownloadController* GetInstance();

  // Called when DownloadController Java object is instantiated.
  void Init(JNIEnv* env, jobject obj);

  // Starts a new download request with Android. Should be called on the
  // UI thread.
  void CreateGETDownload(RenderViewHost* source,
                         int request_id);

  // Should be called when a POST download is started. Notifies the embedding
  // app about the download. Called on the UI thread.
  void OnPostDownloadStarted(WebContents* web_contents,
                             DownloadItem* download_item);

  // DownloadItem::Observer interface.
  virtual void OnDownloadUpdated(DownloadItem* item) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* item) OVERRIDE;

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
    int64 total_bytes;
    std::string content_disposition;
    std::string original_mime_type;
    std::string user_agent;
    std::string cookie;
    std::string referer;

    WebContents* web_contents;
    // Default copy constructor is used for passing this struct by value.
  };

  struct JavaObject;
  friend struct DefaultSingletonTraits<DownloadController>;
  DownloadController();
  virtual ~DownloadController();

  void PrepareDownloadInfo(const GlobalRequestID& global_id,
                           int render_process_id,
                           int render_view_id);

  void CheckPolicyAndLoadCookies(const DownloadInfoAndroid& info,
                                 int render_process_id,
                                 int render_view_id,
                                 const GlobalRequestID& global_id,
                                 const net::CookieList& cookie_list);

  void DoLoadCookies(const DownloadInfoAndroid& info,
                     int render_process_id,
                     int render_view_id,
                     const GlobalRequestID& global_id);

  void OnCookieResponse(DownloadInfoAndroid info,
                        int render_process_id,
                        int render_view_id,
                        const std::string& cookie);

  void StartAndroidDownload(const DownloadInfoAndroid& info,
                            int render_process_id,
                            int render_view_id);

  base::android::ScopedJavaLocalRef<jobject> GetContentViewCoreFromWebContents(
      WebContents* web_contents);

  base::android::ScopedJavaLocalRef<jobject> GetContentView(
      int render_process_id, int render_view_id);

  // Creates Java object if it is not created already and returns it.
  JavaObject* GetJavaObject();

  JavaObject* java_object_;

  DISALLOW_COPY_AND_ASSIGN(DownloadController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DOWNLOAD_CONTROLLER_H_
