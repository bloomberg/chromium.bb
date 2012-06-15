// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CLIENT_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CLIENT_H_
#pragma once

#include "base/android/jni_helper.h"
#include "base/compiler_specific.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

class FindHelper;

namespace content {
class DownloadItem;
struct FindMatchRect;
class JavaScriptDialogCreator;
class NativeWebKeyboardEvent;
class RenderViewHost;
class WebContents;
}

namespace content {

// This enum must be kept in sync with ContentViewClient.java
enum ContentViewClientError {
  // Success
  CONTENT_VIEW_CLIENT_ERROR_OK = 0,
  // Generic error
  CONTENT_VIEW_CLIENT_ERROR_UNKNOWN = -1,
  // Server or proxy hostname lookup failed
  CONTENT_VIEW_CLIENT_ERROR_HOST_LOOKUP = -2,
  // Unsupported authentication scheme (not basic or digest)
  CONTENT_VIEW_CLIENT_ERROR_UNSUPPORTED_AUTH_SCHEME = -3,
  // User authentication failed on server
  CONTENT_VIEW_CLIENT_ERROR_AUTHENTICATION = -4,
  // User authentication failed on proxy
  CONTENT_VIEW_CLIENT_ERROR_PROXY_AUTHENTICATION = -5,
  // Failed to connect to the server
  CONTENT_VIEW_CLIENT_ERROR_CONNECT = -6,
  // Failed to read or write to the server
  CONTENT_VIEW_CLIENT_ERROR_IO = -7,
  // Connection timed out
  CONTENT_VIEW_CLIENT_ERROR_TIMEOUT = -8,
  // Too many redirects
  CONTENT_VIEW_CLIENT_ERROR_REDIRECT_LOOP = -9,
  // Unsupported URI scheme
  CONTENT_VIEW_CLIENT_ERROR_UNSUPPORTED_SCHEME = -10,
  // Failed to perform SSL handshake
  CONTENT_VIEW_CLIENT_ERROR_FAILED_SSL_HANDSHAKE = -11,
  // Malformed URL
  CONTENT_VIEW_CLIENT_ERROR_BAD_URL = -12,
  // Generic file error
  CONTENT_VIEW_CLIENT_ERROR_FILE = -13,
  // File not found
  CONTENT_VIEW_CLIENT_ERROR_FILE_NOT_FOUND = -14,
  // Too many requests during this load
  CONTENT_VIEW_CLIENT_ERROR_TOO_MANY_REQUESTS = -15,
};

// Native mirror of ContentViewClient.java.  Uses as a client of
// ContentView, the main FrameLayout on Android.
class ContentViewClient : public WebContentsDelegate {
 public:
  ContentViewClient(JNIEnv* env, jobject obj);

  static ContentViewClient* CreateNativeContentViewClient(JNIEnv* env,
                                                          jobject obj);

  // Called by ContentView:
  void OnInternalPageLoadRequest(WebContents* source,
                                 const GURL& url);
  void OnPageStarted(const GURL& url);
  void OnPageFinished(const GURL& url);
  void OnLoadStarted();
  void OnLoadStopped();
  void OnReceivedError(int error_code,
                       const string16& description,
                       const GURL& url);
  void OnReceivedHttpAuthRequest(jobject auth_handler,
                                 const string16& host,
                                 const string16& realm);
  void OnDidCommitMainFrame(const GURL& url,
                            const GURL& base_url);
  void OnInterstitialShown();
  void OnInterstitialHidden();

  void SetFindHelper(FindHelper* find_helper);
  void SetJavaScriptDialogCreator(
      JavaScriptDialogCreator* javascript_dialog_creator);

  bool OnJSModalDialog(JavaScriptMessageType type,
                       bool is_before_unload_dialog,
                       const GURL& url,
                       const string16& message,
                       const string16& default_value);

  // Overridden from WebContentsDelegate:
  virtual WebContents* OpenURLFromTab(
      WebContents* source,
      const OpenURLParams& params) OVERRIDE;
  virtual bool ShouldIgnoreNavigation(
      WebContents* source,
      const GURL& url,
      const Referrer& referrer,
      WindowOpenDisposition disposition,
      PageTransition transition_type) OVERRIDE;
  virtual void NavigationStateChanged(const WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNewContents(WebContents* source,
                              WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual void ActivateContents(WebContents* contents) OVERRIDE;
  virtual void DeactivateContents(WebContents* contents) OVERRIDE;
  virtual void LoadingStateChanged(WebContents* source) OVERRIDE;
  virtual void LoadProgressChanged(double load_progress) OVERRIDE;
  virtual void CloseContents(WebContents* source) OVERRIDE;
  virtual void MoveContents(WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  // TODO(merge): WARNING! method no longer available on the base class.
  // See http://b/issue?id=5862108
  virtual void URLStarredChanged(WebContents* source, bool starred);
  virtual void UpdateTargetURL(WebContents* source,
                               int32 page_id,
                               const GURL& url) OVERRIDE;
  virtual bool CanDownload(RenderViewHost* source,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual void OnStartDownload(WebContents* source,
                               DownloadItem* download) OVERRIDE;
  virtual void FindReply(WebContents* tab,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual void OnReceiveFindMatchRects(int version,
      const std::vector<FindMatchRect>& rects,
      const FindMatchRect& active_rect) OVERRIDE;
  virtual bool ShouldOverrideLoading(const GURL& url) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual JavaScriptDialogCreator* GetJavaScriptDialogCreator() OVERRIDE;
  virtual void RunFileChooser(
      WebContents* tab,
      const FileChooserParams& params) OVERRIDE;
  virtual bool TakeFocus(bool reverse) OVERRIDE;

  virtual ~ContentViewClient();

 private:
  // Get the closest ContentViewClient match to the given Chrome error code.
  static ContentViewClientError ToContentViewClientError(int net_error);

  // We use this to keep track of whether the navigation we get in
  // ShouldIgnoreNavigation has been initiated by the ContentView or not.  We
  // need the GURL, because the active navigation entry doesn't change on
  // redirects.
  GURL last_requested_navigation_url_;

  // We depend on ContentView.java to hold a ref to the client object. If we
  // were to hold a hard ref from native we could end up with a cyclic
  // ownership leak (the GC can't collect cycles if part of the cycle is caused
  // by native).
  JavaObjectWeakGlobalRef weak_java_client_;

  // Used to process find replies. Owned by the ContentView.  The ContentView
  // NULLs this pointer when the FindHelper goes away.
  FindHelper* find_helper_;

  // The object responsible for creating JavaScript dialogs.
  JavaScriptDialogCreator* javascript_dialog_creator_;
};

bool RegisterContentViewClient(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CLIENT_H_
