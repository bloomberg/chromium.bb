// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_URL_REQUEST_H_
#define CHROME_FRAME_URLMON_URL_REQUEST_H_

#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>
#include <map>
#include <string>

#include "chrome_frame/plugin_url_request.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/utils.h"

namespace base {
class MessageLoop;
class Thread;
}

class UrlmonUrlRequest;

class UrlmonUrlRequestManager
    : public PluginUrlRequestManager,
      public PluginUrlRequestDelegate {
 public:
  // Contains the privacy information for all requests issued by this instance.
  struct PrivacyInfo {
   public:
    struct PrivacyEntry {
      PrivacyEntry() : flags(0) {}
      std::wstring policy_ref;
      int32 flags;
    };

    typedef std::map<std::wstring, PrivacyEntry> PrivacyRecords;

    PrivacyInfo() : privacy_impacted(false) {}

    bool privacy_impacted;
    PrivacyRecords privacy_records;
  };

  UrlmonUrlRequestManager();
  ~UrlmonUrlRequestManager();

  // Use specific bind context when Chrome request this url.
  // Used from ChromeActiveDocument's implementation of IPersistMoniker::Load().
  void SetInfoForUrl(const std::wstring& url,
                     IMoniker* moniker, LPBC bind_context);

  // Returns a copy of the url privacy information for this instance.
  PrivacyInfo privacy_info() {
    return privacy_info_;
  }

  virtual void AddPrivacyDataForUrl(const std::string& url,
                                    const std::string& policy_ref,
                                    int32 flags);

  // This function passes the window on which notifications are to be fired.
  void put_notification_window(HWND window) {
    notification_window_ = window;
  }

  // This function passes information on whether ChromeFrame is running in
  // privileged mode.
  void set_privileged_mode(bool privileged_mode) {
    privileged_mode_ = privileged_mode;
  }

  void set_container(IUnknown* container) {
    container_ = container;
  }

 private:
  friend class base::MessageLoop;

  // PluginUrlRequestManager implementation.
  virtual PluginUrlRequestManager::ThreadSafeFlags GetThreadSafeFlags();
  virtual void StartRequest(int request_id,
                            const AutomationURLRequest& request_info);
  virtual void ReadRequest(int request_id, int bytes_to_read);
  virtual void EndRequest(int request_id);
  virtual void DownloadRequestInHost(int request_id);
  virtual void StopAll();
  virtual void GetCookiesForUrl(const GURL& url, int cookie_id);
  virtual void SetCookiesForUrl(const GURL& url, const std::string& cookie);

  // PluginUrlRequestDelegate implementation
  virtual void OnResponseStarted(
      int request_id, const char* mime_type, const char* headers, int size,
      base::Time last_modified, const std::string& redirect_url,
      int redirect_status, const net::HostPortPair& socket_address,
      uint64 upload_size);
  virtual void OnReadComplete(int request_id, const std::string& data);
  virtual void OnResponseEnd(int request_id,
                             const net::URLRequestStatus& status);
  virtual void OnCookiesRetrieved(bool success, const GURL& url,
                                  const std::string& cookie_string,
                                  int cookie_id);

  // This method is passed as a callback to UrlmonUrlRequest::TerminateBind.
  // We simply forward moniker and bind_ctx to host ActiveX/ActiveDocument,
  // so it may start NavigateWithBindContext.
  void BindTerminated(IMoniker* moniker, IBindCtx* bind_ctx,
                      IStream* post_data, const char* request_headers);

  // Helper function to initiate a download request in the host.
  void DownloadRequestInHostHelper(UrlmonUrlRequest* request);

  // Map for (request_id <-> UrlmonUrlRequest)
  typedef std::map<int, scoped_refptr<UrlmonUrlRequest> > RequestMap;
  RequestMap request_map_;
  RequestMap background_request_map_;

  // The caller is responsible for acquiring any locks needed to access the
  // request map.
  scoped_refptr<UrlmonUrlRequest> LookupRequest(int request_id,
                                                RequestMap* request_map);
  // The background_request_map_ is referenced from multiple threads. Lock to
  // synchronize access.
  base::Lock background_resource_map_lock_;

  // Helper function to stop all requests in the request map.
  void StopAllRequestsHelper(RequestMap* request_map,
                             base::Lock* request_map_lock);
  // Helper function for initiating a new IE request.
  void StartRequestHelper(int request_id,
                          const AutomationURLRequest& request_info,
                          RequestMap* request_map,
                          base::Lock* request_map_lock);

  scoped_refptr<UrlmonUrlRequest> pending_request_;
  scoped_ptr<base::Thread> background_thread_;

  bool stopping_;

  // Controls whether we download subresources on the page in a background
  // worker thread.
  bool background_worker_thread_enabled_;

  PrivacyInfo privacy_info_;
  // The window to be used to fire notifications on.
  HWND notification_window_;
  // Set to true if the ChromeFrame instance is running in privileged mode.
  bool privileged_mode_;
  // A pointer to the containing object. We maintain a weak reference to avoid
  // lifetime issues.
  IUnknown* container_;
};

#endif  // CHROME_FRAME_URLMON_URL_REQUEST_H_
