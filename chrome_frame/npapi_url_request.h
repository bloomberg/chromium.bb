// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_NPAPI_URL_REQUEST_H_
#define CHROME_FRAME_NPAPI_URL_REQUEST_H_

#include <map>
#include <string>

#include "chrome_frame/plugin_url_request.h"
#include "third_party/npapi/bindings/npapi.h"

class NPAPIUrlRequest;
class NPAPIUrlRequestManager : public PluginUrlRequestManager,
                               public PluginUrlRequestDelegate {
 public:
  NPAPIUrlRequestManager();
  ~NPAPIUrlRequestManager();

  void set_NPPInstance(NPP instance) {
    instance_ = instance;
  }

  // Notifications from the browser. We find the appropriate NPAPIUrlRequest
  // and forward the call.
  NPError NewStream(NPMIMEType type, NPStream* stream,
                    NPBool seekable, uint16* stream_type);
  int32 WriteReady(NPStream* stream);
  int32 Write(NPStream* stream, int32 offset, int32 len, void* buffer);
  NPError DestroyStream(NPStream* stream, NPReason reason);
  void UrlNotify(const char* url, NPReason reason, void* notify_data);
  void UrlRedirectNotify(const char* url, int status, void* notify_data);

 private:
  // PluginUrlRequestManager implementation. Called from AutomationClient.
  virtual PluginUrlRequestManager::ThreadSafeFlags GetThreadSafeFlags();
  virtual void StartRequest(int request_id,
                            const AutomationURLRequest& request_info);
  virtual void ReadRequest(int request_id, int bytes_to_read);
  virtual void EndRequest(int request_id);
  virtual void DownloadRequestInHost(int request_id) {
    // Not yet implemented.
  }
  virtual void StopAll();
  virtual void SetCookiesForUrl(const GURL& url, const std::string& cookie);
  virtual void GetCookiesForUrl(const GURL& url, int cookie_id);

  // Outstanding requests map.
  typedef std::map<int, scoped_refptr<NPAPIUrlRequest> > RequestMap;
  RequestMap request_map_;

  scoped_refptr<NPAPIUrlRequest> LookupRequest(int request_id);

  // PluginUrlRequestDelegate implementation. Forwards back to delegate.
  virtual void OnResponseStarted(int request_id, const char* mime_type,
      const char* headers, int size, base::Time last_modified,
      const std::string& redirect_url, int redirect_status,
      const net::HostPortPair& socket_address);
  virtual void OnReadComplete(int request_id, const std::string& data);
  virtual void OnResponseEnd(int request_id,
                             const net::URLRequestStatus& status);
  virtual void OnCookiesRetrieved(bool success, const GURL& url,
      const std::string& cookie_string, int cookie_id);

  static inline NPAPIUrlRequest* RequestFromNotifyData(void* notify_data) {
    return reinterpret_cast<NPAPIUrlRequest*>(notify_data);
  }

  NPP instance_;
};

#endif  // CHROME_FRAME_NPAPI_URL_REQUEST_H_
