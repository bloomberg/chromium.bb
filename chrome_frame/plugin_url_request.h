// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_PLUGIN_URL_REQUEST_H_
#define CHROME_FRAME_PLUGIN_URL_REQUEST_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/win/scoped_comptr.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/urlmon_upload_data_stream.h"
#include "ipc/ipc_message.h"
#include "net/base/host_port_pair.h"
#include "net/base/upload_data.h"
#include "net/url_request/url_request_status.h"
#include "webkit/common/resource_type.h"

class PluginUrlRequest;
class PluginUrlRequestDelegate;
class PluginUrlRequestManager;

class DECLSPEC_NOVTABLE PluginUrlRequestDelegate {  // NOLINT
 public:
  virtual void AddPrivacyDataForUrl(const std::string& url,
                                    const std::string& policy_ref,
                                    int32 flags) {}
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

 protected:
  PluginUrlRequestDelegate* delegate_;
  bool enable_frame_busting_;
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
  base::win::ScopedComPtr<IStream> upload_data_;
  bool is_chunked_upload_;
  // Contains the ip address and port of the destination host.
  net::HostPortPair socket_address_;
};

#endif  // CHROME_FRAME_PLUGIN_URL_REQUEST_H_
