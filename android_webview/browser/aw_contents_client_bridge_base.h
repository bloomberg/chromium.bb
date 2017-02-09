// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_BASE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_BASE_H_

#include <memory>

#include "android_webview/browser/net/aw_web_resource_request.h"
#include "base/supports_user_data.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/resource_request_info.h"
#include "net/http/http_response_headers.h"

class GURL;

namespace content {
class ClientCertificateDelegate;
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
  // Adds the handler to the UserData registry.
  static void Associate(content::WebContents* web_contents,
                        AwContentsClientBridgeBase* handler);
  static AwContentsClientBridgeBase* FromWebContents(
      content::WebContents* web_contents);
  static AwContentsClientBridgeBase* FromWebContentsGetter(
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter);
  static AwContentsClientBridgeBase* FromID(int render_process_id,
                                            int render_frame_id);

  virtual ~AwContentsClientBridgeBase();

  virtual void AllowCertificateError(
      int cert_error,
      net::X509Certificate* cert,
      const GURL& request_url,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback,
      bool* cancel_request) = 0;
  virtual void SelectClientCertificate(
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) = 0;

  virtual void RunJavaScriptDialog(
      content::JavaScriptDialogType dialog_type,
      const GURL& origin_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const content::JavaScriptDialogManager::DialogClosedCallback&
          callback) = 0;

  virtual void RunBeforeUnloadDialog(
      const GURL& origin_url,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      = 0;

  virtual bool ShouldOverrideUrlLoading(const base::string16& url,
                                        bool has_user_gesture,
                                        bool is_redirect,
                                        bool is_main_frame) = 0;

  virtual void NewDownload(const GURL& url,
                           const std::string& user_agent,
                           const std::string& content_disposition,
                           const std::string& mime_type,
                           int64_t content_length) = 0;

  // Called when a new login request is detected. See the documentation for
  // WebViewClient.onReceivedLoginRequest for arguments. Note that |account|
  // may be empty.
  virtual void NewLoginRequest(const std::string& realm,
                               const std::string& account,
                               const std::string& args) = 0;

  // Called when a resource loading error has occured (e.g. an I/O error,
  // host name lookup failure etc.)
  virtual void OnReceivedError(const AwWebResourceRequest& request,
                               int error_code) = 0;

  // Called when a response from the server is received with status code >= 400.
  virtual void OnReceivedHttpError(
      const AwWebResourceRequest& request,
      const scoped_refptr<const net::HttpResponseHeaders>&
          response_headers) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_BASE_H_
