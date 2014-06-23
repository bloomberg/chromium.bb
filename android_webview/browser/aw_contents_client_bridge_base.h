// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_BASE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_BASE_H_

#include "base/callback_forward.h"
#include "base/supports_user_data.h"
#include "content/public/browser/javascript_dialog_manager.h"

class GURL;

namespace content {
class WebContents;
}

namespace net {
class SSLCertRequestInfo;
class X509Certificate;
}

namespace android_webview {

// browser/ layer interface for AwContensClientBridge, as DEPS prevents this
// layer from depending on native/ where the implementation lives. The
// implementor of the base class plumbs the request to the Java side and
// eventually to the webviewclient. This layering hides the details of
// native/ from browser/ layer.
class AwContentsClientBridgeBase {
 public:
  typedef base::Callback<void(net::X509Certificate*)> SelectCertificateCallback;

  // Adds the handler to the UserData registry.
  static void Associate(content::WebContents* web_contents,
                        AwContentsClientBridgeBase* handler);
  static AwContentsClientBridgeBase* FromWebContents(
      content::WebContents* web_contents);
  static AwContentsClientBridgeBase* FromID(int render_process_id,
                                            int render_frame_id);

  virtual ~AwContentsClientBridgeBase();

  virtual void AllowCertificateError(int cert_error,
                                     net::X509Certificate* cert,
                                     const GURL& request_url,
                                     const base::Callback<void(bool)>& callback,
                                     bool* cancel_request) = 0;
  virtual void SelectClientCertificate(
      net::SSLCertRequestInfo* cert_request_info,
      const SelectCertificateCallback& callback) = 0;

  virtual void RunJavaScriptDialog(
      content::JavaScriptMessageType message_type,
      const GURL& origin_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      = 0;

  virtual void RunBeforeUnloadDialog(
      const GURL& origin_url,
      const base::string16& message_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      = 0;

  virtual bool ShouldOverrideUrlLoading(const base::string16& url) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_BASE_H_
