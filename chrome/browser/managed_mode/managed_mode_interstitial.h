// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_INTERSTITIAL_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_INTERSTITIAL_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "googleurl/src/gurl.h"

namespace content {
class InterstitialPage;
class WebContents;
}

// Delegate for an interstitial page when a page is blocked in managed mode
// because it is not on any whitelist. It lets an authorized user preview the
// blocked page, to decide later whether to manually allow it.
class ManagedModeInterstitial : public content::InterstitialPageDelegate {
 public:
  ManagedModeInterstitial(content::WebContents* web_contents,
                          const GURL& url,
                          const base::Callback<void(bool)>& callback);
  virtual ~ManagedModeInterstitial();

  // Should be called on the IO thread.
  // |render_process_host_id| and |render_view_id| identify the WebContents
  // where the request was blocked. |url| is the URL that was blocked.
  // |callback| should be called with the result (whether to allow the request
  // or not).
  static void ShowInterstitial(int render_process_host_id,
                               int render_view_id,
                               const GURL& url,
                               const base::Callback<void(bool)>& callback);

 private:
  void GoToNewTabPage();

  // InterstitialPageDelegate implementation.
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void OnProceed() OVERRIDE;
  virtual void OnDontProceed() OVERRIDE;

  // Will be called when the passphrase dialog is closed, which is shown after
  // clicking the preview button.
  void OnAuthorizationResult(bool success);
  void DispatchContinueRequest(bool continue_request);

  // Owns the interstitial, which owns us.
  content::WebContents* web_contents_;

  content::InterstitialPage* interstitial_page_;  // Owns us.

  // The UI language. Used for formatting the URL for display.
  std::string languages_;
  GURL url_;

  base::WeakPtrFactory<ManagedModeInterstitial> weak_ptr_factory_;
  base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeInterstitial);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_INTERSTITIAL_H_
