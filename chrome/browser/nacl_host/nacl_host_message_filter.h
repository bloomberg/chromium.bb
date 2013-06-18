// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_HOST_MESSAGE_FILTER_H_
#define CHROME_BROWSER_NACL_HOST_NACL_HOST_MESSAGE_FILTER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_message_filter.h"

class ExtensionInfoMap;
class GURL;
class Profile;

namespace nacl {
struct NaClLaunchParams;
}

namespace net {
class HostResolver;
class URLRequestContextGetter;
}

// This class filters out incoming Chrome-specific IPC messages for the renderer
// process on the IPC thread.
class NaClHostMessageFilter : public content::BrowserMessageFilter {
 public:
  NaClHostMessageFilter(int render_process_id,
                        Profile* profile,
                        net::URLRequestContextGetter* request_context);

  // content::BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  int render_process_id() { return render_process_id_; }
  bool off_the_record() { return off_the_record_; }
  net::HostResolver* GetHostResolver();

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<NaClHostMessageFilter>;

  virtual ~NaClHostMessageFilter();

#if !defined(DISABLE_NACL)
  void OnLaunchNaCl(const nacl::NaClLaunchParams& launch_params,
                    IPC::Message* reply_msg);
  void OnGetReadonlyPnaclFd(const std::string& filename,
                            IPC::Message* reply_msg);
  void OnNaClCreateTemporaryFile(IPC::Message* reply_msg);
  void OnNaClErrorStatus(int render_view_id, int error_id);
  void OnOpenNaClExecutable(int render_view_id,
                            const GURL& file_url,
                            IPC::Message* reply_msg);
#endif
  int render_process_id_;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread!
  Profile* profile_;
  // Copied from the profile so that it can be read on the IO thread.
  bool off_the_record_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;

  base::WeakPtrFactory<NaClHostMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NaClHostMessageFilter);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_HOST_MESSAGE_FILTER_H_
