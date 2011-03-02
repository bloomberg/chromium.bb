// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "googleurl/src/gurl.h"

class TabContents;

namespace safe_browsing {

class ClientSideDetectionService;

// This class is used to receive the IPC from the renderer which
// notifies the browser that a URL was classified as phishing.  This
// class relays this information to the client-side detection service
// class which sends a ping to a server to validate the verdict.
// TODO(noelutz): move all client-side detection IPCs to this class.
class ClientSideDetectionHost : public TabContentsObserver {
 public:
  // The caller keeps ownership of the tab object and is responsible for
  // ensuring that it stays valid for the entire lifetime of this object.
  explicit ClientSideDetectionHost(TabContents* tab);
  virtual ~ClientSideDetectionHost();

  // From TabContentsObserver.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // From TabContentsObserver.  If we navigate away we cancel all pending
  // callbacks that could show an interstitial.
  virtual void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

 private:
  friend class ClientSideDetectionHostTest;

  void OnDetectedPhishingSite(const GURL& phishing_url, double phishing_score);

  // Callback that is called when the server ping back is
  // done. Display an interstitial if |is_phishing| is true.
  // Otherwise, we do nothgin.  Called in UI thread.
  void MaybeShowPhishingWarning(GURL phishing_url, bool is_phishing);

  // Used for testing.  This function does not take ownership of the service
  // class.
  void set_client_side_detection_service(ClientSideDetectionService* service);

  // Used for testing.  This function does not take ownership of the service
  // class.
  void set_safe_browsing_service(SafeBrowsingService* service);

  // This pointer may be NULL if client-side phishing detection is disabled.
  ClientSideDetectionService* service_;
  // This pointer may be NULL if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingService> sb_service_;

  base::ScopedCallbackFactory<ClientSideDetectionHost> cb_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionHost);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
