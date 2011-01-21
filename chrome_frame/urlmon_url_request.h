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

#include "base/threading/thread.h"
#include "chrome_frame/plugin_url_request.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/utils.h"

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
  friend class MessageLoop;
  friend struct RunnableMethodTraits<UrlmonUrlRequestManager>;
  static bool ImplementsThreadSafeReferenceCounting() { return true; }
  void AddRef() {}
  void Release() {}

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
  virtual void OnResponseStarted(int request_id, const char* mime_type,
                                 const char* headers, int size,
                                 base::Time last_modified,
                                 const std::string& redirect_url,
                                 int redirect_status);
  virtual void OnReadComplete(int request_id, const std::string& data);
  virtual void OnResponseEnd(int request_id,
                             const net::URLRequestStatus& status);
  virtual void OnCookiesRetrieved(bool success, const GURL& url,
                                  const std::string& cookie_string,
                                  int cookie_id);

  // This method is passed as a callback to UrlmonUrlRequest::TerminateBind.
  // We simply forward moniker and bind_ctx to host ActiveX/ActiveDocument,
  // so it may start NavigateWithBindContext.
  void BindTerminated(IMoniker* moniker, IBindCtx* bind_ctx);

  // Map for (request_id <-> UrlmonUrlRequest)
  typedef std::map<int, scoped_refptr<UrlmonUrlRequest> > RequestMap;
  RequestMap request_map_;
  scoped_refptr<UrlmonUrlRequest> LookupRequest(int request_id);
  scoped_refptr<UrlmonUrlRequest> pending_request_;

  bool stopping_;
  int calling_delegate_;  // re-entrancy protection (debug only check)

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
