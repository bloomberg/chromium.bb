// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_MERGE_SESSION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_MERGE_SESSION_THROTTLE_H_

#include <set>

#include "base/atomic_ref_count.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"
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
  // Passed a boolean indicating whether or not it is OK to proceed with the
  // page load.
  typedef base::Closure CompletionCallback;

  explicit MergeSessionThrottle(net::URLRequest* request,
                                content::ResourceType::Type resource_type);
  virtual ~MergeSessionThrottle();

  // content::ResourceThrottle implementation:
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual const char* GetNameForLogging() const OVERRIDE;

  // Checks if session is already merged.
  static bool AreAllSessionMergedAlready();

 private:

  // MergeSessionLoadPage callback.
  void OnBlockingPageComplete();

  // Erase the state associated with a deferred load request.
  void ClearRequestInfo();
  bool IsRemote(const GURL& url) const;

  // True if we |url| loading should be delayed. The function
  // is safe to be called on any thread.
  bool ShouldDelayUrl(const GURL& url) const;

  // Adds/removes |profile| to/from the blocking profiles set.
  static void BlockProfile(Profile* profile);
  static void UnblockProfile(Profile* profile);

  // Helper method that checks if we should delay reasource loading based on
  // the state of the Profile that's derived from |render_process_id| and
  // |render_view_id|.
  static bool ShouldDelayRequest(int render_process_id,
                                 int render_view_id);

  // Tests merge session status and if needed generates request
  // waiter (for ResourceType::XHR content) or shows interstitial page
  // (for ResourceType::MAIN_FRAME).
  // The function must be called from UI thread.
  static void DeleayResourceLoadingOnUIThread(
      content::ResourceType::Type resource_type,
      int render_process_id,
      int render_view_id,
      const GURL& url,
      const MergeSessionThrottle::CompletionCallback& callback);

  net::URLRequest* request_;
  content::ResourceType::Type resource_type_;

  // Global counter that keeps the track of session merge status for all
  // encountered profiles. This is used to determine if a throttle should
  // even be even added to new requests. Value of 0 (initial) means that we
  // probably have some profiles to restore, while 1 means that all known
  // profiles are restored.
  static base::AtomicRefCount all_profiles_restored_;

  DISALLOW_COPY_AND_ASSIGN(MergeSessionThrottle);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_MERGE_SESSION_THROTTLE_H_
