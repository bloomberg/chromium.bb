// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_URL_REQUEST_H_
#define CHROME_FRAME_URLMON_URL_REQUEST_H_

#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>
#include <map>
#include <string>

#include "base/lock.h"
#include "base/scoped_comptr_win.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome_frame/plugin_url_request.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/utils.h"

class UrlmonUrlRequest;

class RequestDataForUrl {
 public:
  RequestDataForUrl(RequestData* data, const std::wstring& url)
      : request_data_(data), url_(url) {
  }
  ~RequestDataForUrl() {
  }

  const std::wstring& url() const {
    return url_;
  }

  RequestData* request_data() const {
    return request_data_;
  }

 protected:
  scoped_refptr<RequestData> request_data_;
  std::wstring url_;
};

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

  // Use cached data when Chrome request this url.
  // Used from ChromeActiveDocument's implementation of IPersistMoniker::Load().
  void UseRequestDataForUrl(RequestData* data, const std::wstring& url);

  // Returns a copy of the url privacy information for this instance.
  PrivacyInfo privacy_info() {
    AutoLock lock(privacy_info_lock_);
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

 private:
  friend class MessageLoop;
  friend struct RunnableMethodTraits<UrlmonUrlRequestManager>;
  static bool ImplementsThreadSafeReferenceCounting() { return true; }
  void AddRef() {}
  void Release() {}

  // PluginUrlRequestManager implementation.
  virtual bool IsThreadSafe();
  virtual void StartRequest(int request_id,
                            const IPC::AutomationURLRequest& request_info);
  virtual void ReadRequest(int request_id, int bytes_to_read);
  virtual void EndRequest(int request_id);
  virtual void DownloadRequestInHost(int request_id);
  virtual void StopAll();

  // PluginUrlRequestDelegate implementation
  virtual void OnResponseStarted(int request_id, const char* mime_type,
                                 const char* headers, int size,
                                 base::Time last_modified,
                                 const std::string& redirect_url,
                                 int redirect_status);
  virtual void OnReadComplete(int request_id, const void* buffer, int len);
  virtual void OnResponseEnd(int request_id, const URLRequestStatus& status);

  bool ExecuteInWorkerThread(const tracked_objects::Location& from_here,
                             Task* task);
  // Methods executed in worker thread.
  void StartRequestWorker(int request_id,
                          const IPC::AutomationURLRequest& request_info,
                          RequestDataForUrl* use_request);
  void ReadRequestWorker(int request_id, int bytes_to_read);
  void EndRequestWorker(int request_id);
  void StopAllWorker();

  // Map for (request_id <-> UrlmonUrlRequest)
  typedef std::map<int, scoped_refptr<UrlmonUrlRequest> > RequestMap;
  RequestMap request_map_;
  scoped_refptr<UrlmonUrlRequest> LookupRequest(int request_id);

  scoped_ptr<RequestDataForUrl> request_data_for_url_;
  STAThread worker_thread_;
  base::WaitableEvent map_empty_;
  bool stopping_;
  Lock worker_thread_access_;

  // This lock is used to synchronize access to the PrivacyInfo data structure
  // as it can be accessed from the ui thread and the worker thread.
  Lock privacy_info_lock_;
  PrivacyInfo privacy_info_;
  // The window to be used to fire notifications on.
  HWND notification_window_;
  // Set to true if the ChromeFrame instance is running in privileged mode.
  bool privileged_mode_;
};

#endif  // CHROME_FRAME_URLMON_URL_REQUEST_H_

