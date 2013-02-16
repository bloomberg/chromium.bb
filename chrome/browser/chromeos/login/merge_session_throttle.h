// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_THROTTLE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/merge_session_load_page.h"
#include "content/public/browser/resource_throttle.h"
#include "net/base/completion_callback.h"

namespace net {
class URLRequest;
}

// Used to show an interstitial page while merge session process (cookie
// reconstruction from OAuth2 refresh token in ChromeOS login) is still in
// progress while we are attempting to load a google property.
class MergeSessionThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<MergeSessionThrottle> {
 public:
  MergeSessionThrottle(int render_process_id,
                       int render_view_id,
                       net::URLRequest* request);
  virtual ~MergeSessionThrottle();

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;

 private:
  // MergeSessionLoadPage callback.
  void OnBlockingPageComplete();

  // Erase the state associated with a deferred load request.
  void ClearRequestInfo();
  bool IsRemote(const GURL& url) const;

  // True if we should show the merge session in progress page.
  bool ShouldShowMergeSessionPage(const GURL& url) const;

  int render_process_id_;
  int render_view_id_;
  net::URLRequest* request_;

  DISALLOW_COPY_AND_ASSIGN(MergeSessionThrottle);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_THROTTLE_H_
