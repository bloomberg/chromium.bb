// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_NTP_PRERENDERER_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_NTP_PRERENDERER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/ui/search/instant_page.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"

class InstantNTP;
class InstantService;
class PrefService;
class Profile;

// InstantNTPPrerenderer maintains a prerendered instance of InstantNTP.
//
// An InstantNTP instance is a preloaded search page that will be swapped-in the
// next time when the user navigates to the New Tab Page. It is never shown to
// the user in an uncommitted state. It is backed by a WebContents and that is
// owned by InstantNTP.
//
// InstantNTPPrerenderer is owned by InstantService.
class InstantNTPPrerenderer
    : public InstantPage::Delegate,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public InstantServiceObserver {
 public:
  InstantNTPPrerenderer(Profile* profile, InstantService* instant_service,
      PrefService* prefs);
  virtual ~InstantNTPPrerenderer();

  // Preloads |ntp_| with a new InstantNTP.
  void ReloadInstantNTP();

  // Releases and returns the InstantNTP WebContents. May be NULL. Loads a new
  // WebContents for the InstantNTP.
  scoped_ptr<content::WebContents> ReleaseNTPContents() WARN_UNUSED_RESULT;

  // The NTP WebContents. May be NULL. InstantNTPPrerenderer retains ownership.
  content::WebContents* GetNTPContents() const;

  // Invoked to null out |ntp_|.
  void DeleteNTPContents();

  // Invoked when the InstantNTP renderer process crashes.
  void RenderProcessGone();

  // Invoked when the |ntp_| main frame load completes.
  void LoadCompletedMainFrame();

 protected:
  // Returns the local Instant URL. (Just a convenience wrapper to get the local
  // Instant URL from InstantService.)
  virtual std::string GetLocalInstantURL() const;

  // Returns the correct Instant URL to use from the following possibilities:
  //     o The default search engine's Instant URL.
  //     o The local page (see GetLocalInstantURL())
  // Returns an empty string if no valid Instant URL is available (this is only
  // possible in non-extended mode where we don't have a local page fall-back).
  virtual std::string GetInstantURL() const;

  // Returns true if Javascript is enabled and false otherwise.
  virtual bool IsJavascriptEnabled() const;

  // Returns true if the browser is in startup.
  virtual bool InStartup() const;

  // Accessors are made protected for testing purposes.
  virtual InstantNTP* ntp() const;

  Profile* profile() const {
    return profile_;
  }

 private:
  friend class InstantExtendedTest;
  friend class InstantNTPPrerendererTest;
  friend class InstantTestBase;

  FRIEND_TEST_ALL_PREFIXES(InstantExtendedNetworkTest,
                           NTPReactsToNetworkChanges);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           PrefersRemoteNTPOnStartup);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           SwitchesToLocalNTPIfNoInstantSupport);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           SwitchesToLocalNTPIfPathBad);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           DoesNotSwitchToLocalNTPIfOnCurrentNTP);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           DoesNotSwitchToLocalNTPIfOnLocalNTP);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           SwitchesToLocalNTPIfJSDisabled);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           SwitchesToLocalNTPIfNoNTPReady);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           IsJavascriptEnabled);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           IsJavascriptEnabledChecksContentSettings);
  FRIEND_TEST_ALL_PREFIXES(InstantNTPPrerendererTest,
                           IsJavascriptEnabledChecksPrefs);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedManualTest, MANUAL_ShowsGoogleNTP);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedManualTest,
                           MANUAL_SearchesFromFakebox);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ProcessIsolation);

  // Overridden from net::NetworkChangeNotifier::NetworkChangeObserver:
  // If the network status changes, resets InstantNTP.
  virtual void OnNetworkChanged(net::NetworkChangeNotifier::ConnectionType type)
      OVERRIDE;

  // Overridden from InstantPage::Delegate:
  virtual void InstantSupportDetermined(const content::WebContents* contents,
                                        bool supports_instant) OVERRIDE;
  virtual void InstantPageAboutToNavigateMainFrame(
      const content::WebContents* contents,
      const GURL& url) OVERRIDE;
  virtual void FocusOmnibox(const content::WebContents* contents,
                            OmniboxFocusState state) OVERRIDE;
  virtual void NavigateToURL(const content::WebContents* contents,
                             const GURL& url,
                             content::PageTransition transition,
                             WindowOpenDisposition disposition,
                             bool is_search_type) OVERRIDE;
  virtual void PasteIntoOmnibox(const content::WebContents* contents,
                                const string16& text) OVERRIDE;
  virtual void InstantPageLoadFailed(content::WebContents* contents) OVERRIDE;

  // Overridden from InstantServiceObserver:
  virtual void DefaultSearchProviderChanged() OVERRIDE;
  virtual void GoogleURLUpdated() OVERRIDE;

  // Recreates |ntp_| using |instant_url|.
  void ResetNTP(const std::string& instant_url);

  // Returns true if |ntp_| has an up-to-date Instant URL and supports Instant.
  // Note that local URLs will not pass this check.
  bool PageIsCurrent() const;

  // Returns true if we should switch to using the local NTP.
  bool ShouldSwitchToLocalNTP() const;

  Profile* profile_;

  // Preloaded InstantNTP.
  scoped_ptr<InstantNTP> ntp_;

  PrefChangeRegistrar profile_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(InstantNTPPrerenderer);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_NTP_PRERENDERER_H_
