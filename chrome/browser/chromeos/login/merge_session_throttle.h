// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_THROTTLE_H_

#include <set>

#include "base/atomic_ref_count.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/merge_session_load_page.h"
#include "content/public/browser/resource_throttle.h"
#include "net/base/completion_callback.h"

class Profile;

namespace net {
class URLRequest;
}

namespace chromeos {
class OAuth2LoginManager;
}

// Used to show an interstitial page while merge session process (cookie
// reconstruction from OAuth2 refresh token in ChromeOS login) is still in
// progress while we are attempting to load a google property.
class MergeSessionThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<MergeSessionThrottle> {
 public:
  explicit MergeSessionThrottle(net::URLRequest* request);
  virtual ~MergeSessionThrottle();

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;

  // Checks if session is already merged.
  static bool AreAllSessionMergedAlready();

 private:

  // MergeSessionLoadPage callback.
  void OnBlockingPageComplete();

  // Erase the state associated with a deferred load request.
  void ClearRequestInfo();
  bool IsRemote(const GURL& url) const;

  // True if we should show the merge session in progress page. The function
  // is safe to be called on any thread.
  bool ShouldShowMergeSessionPage(const GURL& url) const;

  // Adds/removes |profile| to/from the blocking profiles set.
  static void BlockProfile(Profile* profile);
  static void UnblockProfile(Profile* profile);

  // Helper method that checks if we should show interstitial page based on
  // the state of the Profile that's derived from |render_process_id| and
  // |render_view_id|.
  static bool ShouldShowInterstitialPage(int render_process_id,
                                         int render_view_id);

  // Tests merge session status and if needed shows interstitial page.
  // The function must be called from UI thread.
  static void ShowDeleayedLoadingPageOnUIThread(
      int render_process_id,
      int render_view_id,
      const GURL& url,
      const chromeos::MergeSessionLoadPage::CompletionCallback& callback);

  net::URLRequest* request_;

  // Global counter that keeps the track of session merge status for all
  // encountered profiles. This is used to determine if a throttle should
  // even be even added to new requests. Value of 0 (initial) means that we
  // probably have some profiles to restore, while 1 means that all known
  // profiles are restored.
  static base::AtomicRefCount all_profiles_restored_;

  DISALLOW_COPY_AND_ASSIGN(MergeSessionThrottle);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_THROTTLE_H_
