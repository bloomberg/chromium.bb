// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_PLUGIN_URL_REQUEST_H_
#define CHROME_FRAME_PLUGIN_URL_REQUEST_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_comptr_win.h"
#include "base/time.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/urlmon_upload_data_stream.h"
#include "ipc/ipc_message.h"
#include "net/base/upload_data.h"
#include "net/url_request/url_request_status.h"
#include "webkit/glue/resource_type.h"

class PluginUrlRequest;
class PluginUrlRequestDelegate;
class PluginUrlRequestManager;

class DECLSPEC_NOVTABLE PluginUrlRequestDelegate {  // NOLINT
 public:
  virtual void OnResponseStarted(int request_id, const char* mime_type,
    const char* headers, int size, base::Time last_modified,
    const std::string& redirect_url, int redirect_status) = 0;
  virtual void OnReadComplete(int request_id, const std::string& data) = 0;
  virtual void OnResponseEnd(int request_id,
                             const net::URLRequestStatus& status) = 0;
  virtual void AddPrivacyDataForUrl(const std::string& url,
                                    const std::string& policy_ref,
                                    int32 flags) {}
  virtual void OnCookiesRetrieved(bool success, const GURL& url,
                                  const std::string& cookie_string,
                                  int cookie_id) = 0;
 protected:
  PluginUrlRequestDelegate() {}
  ~PluginUrlRequestDelegate() {}
};

class DECLSPEC_NOVTABLE PluginUrlRequestManager {  // NOLINT
 public:
  PluginUrlRequestManager() : delegate_(NULL), enable_frame_busting_(true) {}
  virtual ~PluginUrlRequestManager() {}

  void set_frame_busting(bool enable) {
    enable_frame_busting_ = enable;
  }

  virtual void set_delegate(PluginUrlRequestDelegate* delegate) {
    delegate_ = delegate;
  }

  enum ThreadSafeFlags {
    NOT_THREADSAFE               = 0x00,
    START_REQUEST_THREADSAFE     = 0x01,
    STOP_REQUEST_THREADSAFE      = 0x02,
    READ_REQUEST_THREADSAFE      = 0x04,
    DOWNLOAD_REQUEST_THREADSAFE  = 0x08,
    COOKIE_REQUEST_THREADSAFE    = 0x10
  };
  virtual ThreadSafeFlags GetThreadSafeFlags() = 0;

  // These are called directly from Automation Client when network related
  // automation messages are received from Chrome.
  // Strip 'tab' handle and forward to the virtual methods implemented by
  // derived classes.
  void StartUrlRequest(int request_id,
                       const AutomationURLRequest& request_info) {
    StartRequest(request_id, request_info);
  }

  void ReadUrlRequest(int request_id, int bytes_to_read) {
    ReadRequest(request_id, bytes_to_read);
  }

  void EndUrlRequest(int request_id, const net::URLRequestStatus& s) {
    EndRequest(request_id);
  }

  void DownloadUrlRequestInHost(int request_id) {
    DownloadRequestInHost(request_id);
  }

  void StopAllRequests() {
    StopAll();
  }

  void GetCookiesFromHost(const GURL& url, int cookie_id) {
    GetCookiesForUrl(url, cookie_id);
  }

  void SetCookiesInHost(const GURL& url, const std::string& cookie) {
    SetCookiesForUrl(url, cookie);
  }

 protected:
  PluginUrlRequestDelegate* delegate_;
  bool enable_frame_busting_;

 private:
  virtual void StartRequest(
      int request_id, const AutomationURLRequest& request_info) = 0;
  virtual void ReadRequest(int request_id, int bytes_to_read) = 0;
  virtual void EndRequest(int request_id) = 0;
  virtual void DownloadRequestInHost(int request_id) = 0;
  virtual void StopAll() = 0;
  virtual void GetCookiesForUrl(const GURL& url, int cookie_id) = 0;
  virtual void SetCookiesForUrl(const GURL& url, const std::string& cookie) = 0;
};

// Used as base class. Holds Url request properties (url, method, referrer..)
class PluginUrlRequest {
 public:
  PluginUrlRequest();
  ~PluginUrlRequest();

  bool Initialize(PluginUrlRequestDelegate* delegate,
      int remote_request_id, const std::string& url, const std::string& method,
      const std::string& referrer, const std::string& extra_headers,
      net::UploadData* upload_data, ResourceType::Type resource_type,
      bool enable_frame_busting_, int load_flags);

  // Accessors.
  int id() const {
    return remote_request_id_;
  }

  const std::string& url() const {
    return url_;
  }

  const std::string& method() const {
    return method_;
  }

  const std::string& referrer() const {
    return referrer_;
  }

  const std::string& extra_headers() const {
    return extra_headers_;
  }

  uint64 post_data_len() const {
    return post_data_len_;
  }

  bool is_chunked_upload() const {
    return is_chunked_upload_;
  }

 protected:
  HRESULT get_upload_data(IStream** ret) {
    DCHECK(ret);
    if (!upload_data_.get())
      return S_FALSE;
    *ret = upload_data_.get();
    (*ret)->AddRef();
    return S_OK;
  }

  void set_url(const std::string& url) {
    url_ = url;
  }

  void ClearPostData() {
    upload_data_.Release();
    post_data_len_ = 0;
  }

  void SendData();
  bool enable_frame_busting_;

  PluginUrlRequestDelegate* delegate_;
  int remote_request_id_;
  uint64 post_data_len_;
  std::string url_;
  std::string method_;
  std::string referrer_;
  std::string extra_headers_;
  ResourceType::Type resource_type_;
  int load_flags_;
  ScopedComPtr<IStream> upload_data_;
  bool is_chunked_upload_;
};

#endif  // CHROME_FRAME_PLUGIN_URL_REQUEST_H_
