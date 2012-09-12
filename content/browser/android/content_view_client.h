// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CLIENT_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CLIENT_H_

#include "base/android/jni_helper.h"
#include "base/compiler_specific.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

namespace content {

class DownloadItem;
class JavaScriptDialogCreator;
struct NativeWebKeyboardEvent;
class RenderViewHost;
class WebContents;

// These enums must be kept in sync with ContentViewClient.java
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

// Native mirror of ContentViewClient.java.  Used as a client of
// ContentView, the main FrameLayout on Android.
// TODO(joth): Delete this C++ class, to make it Java-only. All the callbacks
// defined here originate in WebContentsObserver; we should have a dedicated
// bridge class for that rather than overloading ContentViewClient with this.
// See http://crbug.com/137967
class ContentViewClient {
 public:
  ContentViewClient(JNIEnv* env, jobject obj);
  ~ContentViewClient();

  static ContentViewClient* CreateNativeContentViewClient(JNIEnv* env,
                                                          jobject obj);

  // Called by ContentView:
  void OnPageStarted(const GURL& url);
  void OnPageFinished(const GURL& url);
  void OnLoadStarted();
  void OnLoadStopped();
  void OnReceivedError(int error_code,
                       const string16& description,
                       const GURL& url);
  void OnDidCommitMainFrame(const GURL& url,
                            const GURL& base_url);
  void OnInterstitialShown();
  void OnInterstitialHidden();

  void SetJavaScriptDialogCreator(
      JavaScriptDialogCreator* javascript_dialog_creator);

  bool OnJSModalDialog(JavaScriptMessageType type,
                       bool is_before_unload_dialog,
                       const GURL& url,
                       const string16& message,
                       const string16& default_value);

 private:
  // Get the closest ContentViewClient match to the given Chrome error code.
  static ContentViewClientError ToContentViewClientError(int net_error);

  // We depend on ContentView.java to hold a ref to the client object. If we
  // were to hold a hard ref from native we could end up with a cyclic
  // ownership leak (the GC can't collect cycles if part of the cycle is caused
  // by native).
  JavaObjectWeakGlobalRef weak_java_client_;
};

bool RegisterContentViewClient(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CLIENT_H_
