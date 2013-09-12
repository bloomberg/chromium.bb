// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_LOAD_PAGE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_LOAD_PAGE_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/oauth2_login_manager.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace content {
class InterstitialPage;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace chromeos {

// MergeSessionLoadPage class shows the interstitial page that is shown
// while we are trying to restore session containing tabs with Google properties
// during the process of exchanging OAuth2 refresh token for user cookies.
// It deletes itself when the interstitial page is closed.
class MergeSessionLoadPage
    : public content::InterstitialPageDelegate,
      public OAuth2LoginManager::Observer {
 public:
  // Passed a boolean indicating whether or not it is OK to proceed with the
  // page load.
  typedef base::Closure CompletionCallback;

  // Create a merge session load delay page for the |web_contents|.
  // The |callback| will be run on the IO thread.
  MergeSessionLoadPage(content::WebContents* web_contents,
                       const GURL& url,
                       const CompletionCallback& callback);

  void Show();

 protected:
  virtual ~MergeSessionLoadPage();

 private:
  friend class TestMergeSessionLoadPage;

  // InterstitialPageDelegate implementation.
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void OverrideRendererPrefs(
      content::RendererPreferences* prefs) OVERRIDE;
  virtual void OnProceed() OVERRIDE;
  virtual void OnDontProceed() OVERRIDE;

  // OAuth2LoginManager::Observer overrides.
  virtual void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) OVERRIDE;

  void NotifyBlockingPageComplete();

  // Helper function to get OAuth2LoginManager out of |web_contents_|.
  OAuth2LoginManager* GetOAuth2LoginManager();

  CompletionCallback callback_;

  // True if the proceed is chosen.
  bool proceeded_;

  content::WebContents* web_contents_;
  GURL url_;
  content::InterstitialPage* interstitial_page_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(MergeSessionLoadPage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MERGE_SESSION_LOAD_PAGE_H_
