// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_PLUGIN_URL_REQUEST_H_
#define CHROME_FRAME_PLUGIN_URL_REQUEST_H_

#include <string>
#include <vector>

#include "base/time.h"
#include "ipc/ipc_message.h"
#include "net/base/upload_data.h"
#include "net/url_request/url_request_status.h"
#include "base/ref_counted.h"
#include "chrome_frame/chrome_frame_delegate.h"

class PluginUrlRequest;

// Interface for a class that keeps a collection of outstanding
// reqeusts and offers an outgoing channel.
class PluginRequestHandler : public IPC::Message::Sender {
 public:
  virtual bool AddRequest(PluginUrlRequest* request) = 0;
  virtual void RemoveRequest(PluginUrlRequest* request) = 0;
};

// A reference counting solution that's compatible with COM IUnknown
class UrlRequestReference {
 public:
  virtual unsigned long API_CALL AddRef() = 0;
  virtual unsigned long API_CALL Release() = 0;
};

class PluginUrlRequest : public UrlRequestReference {
 public:
  PluginUrlRequest();
  ~PluginUrlRequest();

  bool Initialize(PluginRequestHandler* handler, int tab,
                  int remote_request_id, const std::string& url,
                  const std::string& method, const std::string& referrer,
                  const std::string& extra_headers,
                  net::UploadData* upload_data,
                  bool intercept_frame_options);

  // Called in response to automation IPCs
  virtual bool Start() = 0;
  virtual void Stop() = 0;
  virtual bool Read(int bytes_to_read) = 0;

  // Persistent cookies are read from the host browser and passed off to Chrome
  // These cookies are sent when we receive a response for every URL request
  // initiated by Chrome. Ideally we should only send cookies for the top level
  // URL and any subframes. However we don't receive information from Chrome
  // about the context for a URL, i.e. whether it is a subframe, etc.
  // Additionally cookies for a URL should be sent once for the page. This
  // is not done now as it is difficult to track URLs, specifically if they
  // are redirected, etc.
  void OnResponseStarted(const char* mime_type, const char* headers, int size,
      base::Time last_modified, const std::string& peristent_cookies,
      const std::string& redirect_url, int redirect_status);

  void OnReadComplete(const void* buffer, int len);
  void OnResponseEnd(const URLRequestStatus& status);

  PluginRequestHandler* request_handler() const {
    return request_handler_;
  }
  int id() const {
    return remote_request_id_;
  }
  const std::string& url() const {
    return url_;
  }
  const std::string& method() const {
    return method_;
  }

  void set_method(const std::string& new_method) {
    method_ = new_method;
  }

  const std::string& referrer() const {
    return referrer_;
  }
  const std::string& extra_headers() const {
    return extra_headers_;
  }
  scoped_refptr<net::UploadData> upload_data() {
    return upload_data_;
  }

  bool is_done() const {
    return (URLRequestStatus::IO_PENDING != status_);
  }

  void set_url(const std::string& url) {
    url_ = url;
  }

 protected:
  void SendData();
  bool frame_busting_enabled_;

 private:
  PluginRequestHandler* request_handler_;
  int tab_;
  int remote_request_id_;
  std::string url_;
  std::string method_;
  std::string referrer_;
  std::string extra_headers_;
  scoped_refptr<net::UploadData> upload_data_;
  URLRequestStatus::Status status_;
};


#endif  // CHROME_FRAME_PLUGIN_URL_REQUEST_H_

