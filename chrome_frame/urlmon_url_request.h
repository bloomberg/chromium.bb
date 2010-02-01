// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_URL_REQUEST_H_
#define CHROME_FRAME_URLMON_URL_REQUEST_H_

#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>
#include <string>

#include "base/scoped_comptr_win.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome_frame/plugin_url_request.h"
#include "chrome_frame/utils.h"

class UrlmonUrlRequest;

class UrlmonUrlRequestManager :
    public PluginUrlRequestManager,
    public PluginUrlRequestDelegate {
 public:
  UrlmonUrlRequestManager();
  ~UrlmonUrlRequestManager();

  // Use specific moniker and bind context when Chrome request this url.
  // Used from ChromeActiveDocument's implementation of IPersistMoniker::Load().
  void UseMonikerForUrl(IMoniker* moniker, IBindCtx* bind_ctx,
                        const std::wstring& url);
  void StealMonikerFromRequest(int request_id, IMoniker** moniker);

 private:
   struct MonikerForUrl {
     ScopedComPtr<IMoniker> moniker;
     ScopedComPtr<IBindCtx> bind_ctx;
     std::wstring url;
   };

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
  virtual void StopAll();

  // PluginUrlRequestDelegate implementation
  virtual void OnResponseStarted(int request_id, const char* mime_type,
    const char* headers, int size, base::Time last_modified,
    const std::string& peristent_cookies, const std::string& redirect_url,
    int redirect_status);
  virtual void OnReadComplete(int request_id, const void* buffer, int len);
  virtual void OnResponseEnd(int request_id, const URLRequestStatus& status);

  // Methods executed in worker thread.
  void StartRequestWorker(int request_id,
                          const IPC::AutomationURLRequest& request_info,
                          MonikerForUrl* moniker_for_url);
  void ReadRequestWorker(int request_id, int bytes_to_read);
  void EndRequestWorker(int request_id);
  void StopAllWorker();
  void StealMonikerFromRequestWorker(int request_id, IMoniker** moniker,
                                     base::WaitableEvent* done);

  // Map for (request_id <-> UrlmonUrlRequest)
  typedef std::map<int, scoped_refptr<UrlmonUrlRequest> > RequestMap;
  RequestMap request_map_;
  scoped_refptr<UrlmonUrlRequest> LookupRequest(int request_id);

  scoped_ptr<MonikerForUrl> moniker_for_url_;
  STAThread worker_thread_;
  base::WaitableEvent map_empty_;
  bool stopping_;
};

#endif  // CHROME_FRAME_URLMON_URL_REQUEST_H_

