// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Classes for managing the SafeBrowsing interstitial pages.
//
// When a user is about to visit a page the SafeBrowsing system has deemed to
// be malicious, either as malware or a phishing page, we show an interstitial
// page with some options (go back, continue) to give the user a chance to avoid
// the harmful page.
//
// The SafeBrowsingBlockingPage is created by the SafeBrowsingUIManager on the
// UI thread when we've determined that a page is malicious. The operation of
// the blocking page occurs on the UI thread, where it waits for the user to
// make a decision about what to do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// SafeBrowsingUIManager so that we can cancel the request for the new page,
// or allow it to continue.
//
// A web page may contain several resources flagged as malware/phishing.  This
// results into more than one interstitial being shown.  On the first unsafe
// resource received we show an interstitial.  Any subsequent unsafe resource
// notifications while the first interstitial is showing is queued.  If the user
// decides to proceed in the first interstitial, we display all queued unsafe
// resources in a new interstitial.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ios/chrome/browser/interstitials/ios_security_interstitial_page.h"
#include "ios/chrome/browser/safe_browsing/ui_manager.h"
#include "url/gurl.h"

namespace web {
class WebState;
}

namespace safe_browsing {

class SafeBrowsingBlockingPageFactory;

class SafeBrowsingBlockingPage : public IOSSecurityInterstitialPage {
 public:
  typedef SafeBrowsingUIManager::UnsafeResource UnsafeResource;
  typedef std::vector<UnsafeResource> UnsafeResourceList;
  typedef std::map<web::WebState*, UnsafeResourceList> UnsafeResourceMap;

  ~SafeBrowsingBlockingPage() override;

  // Creates a blocking page. Use ShowBlockingPage if you don't need to access
  // the blocking page directly.
  static SafeBrowsingBlockingPage* CreateBlockingPage(
      SafeBrowsingUIManager* ui_manager,
      web::WebState* web_state,
      const UnsafeResource& unsafe_resource);

  // Shows a blocking page warning the user about phishing/malware for a
  // specific resource.
  // You can call this method several times, if an interstitial is already
  // showing, the new one will be queued and displayed if the user decides
  // to proceed on the currently showing interstitial.
  static void ShowBlockingPage(web::WebState* web_state,
                               SafeBrowsingUIManager* ui_manager,
                               const UnsafeResource& resource);

  // Makes the passed |factory| the factory used to instantiate
  // SafeBrowsingBlockingPage objects. Useful for tests.
  static void RegisterFactory(SafeBrowsingBlockingPageFactory* factory) {
    factory_ = factory;
  }

  // InterstitialPageDelegate method:
  void OnProceed() override;
  void OnDontProceed() override;
  void CommandReceived(const std::string& command) override;

 protected:
  void UpdateReportingPref();  // Used for the transition from old to new pref.

  // Don't instantiate this class directly, use ShowBlockingPage instead.
  SafeBrowsingBlockingPage(SafeBrowsingUIManager* ui_manager,
                           web::WebState* web_state,
                           const UnsafeResourceList& unsafe_resources);

  // IOSSecurityInterstitialPage methods:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) const override;
  void AfterShow() override {}

  // A list of SafeBrowsingUIManager::UnsafeResource for a tab that the user
  // should be warned about.  They are queued when displaying more than one
  // interstitial at a time.
  static UnsafeResourceMap* GetUnsafeResourcesMap();

  // Returns true if the passed |unsafe_resources| is blocking the load of
  // the main page.
  static bool IsMainPageLoadBlocked(const UnsafeResourceList& unsafe_resources);

  friend class SafeBrowsingBlockingPageFactoryImpl;

  // For reporting back user actions.
  SafeBrowsingUIManager* ui_manager_;

  // True if the interstitial is blocking the main page because it is on one
  // of our lists.  False if a subresource is being blocked, or in the case of
  // client-side detection where the interstitial is shown after page load
  // finishes.
  bool is_main_frame_load_blocked_;

  // The index of a navigation entry that should be removed when DontProceed()
  // is invoked, -1 if not entry should be removed.
  int navigation_entry_index_to_remove_;

  // The list of unsafe resources this page is warning about.
  UnsafeResourceList unsafe_resources_;

  bool proceeded_;

  // Which type of Safe Browsing interstitial this is.
  enum SBInterstitialReason {
    SB_REASON_MALWARE,
    SB_REASON_HARMFUL,
    SB_REASON_PHISHING,
  } interstitial_reason_;

  // The factory used to instantiate SafeBrowsingBlockingPage objects.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingBlockingPage.
  static SafeBrowsingBlockingPageFactory* factory_;

 private:
  // Fills the passed dictionary with the values to be passed to the template
  // when creating the HTML.
  void PopulateMalwareLoadTimeData(base::DictionaryValue* load_time_data) const;
  void PopulateHarmfulLoadTimeData(base::DictionaryValue* load_time_data) const;
  void PopulatePhishingLoadTimeData(
      base::DictionaryValue* load_time_data) const;

  std::string GetMetricPrefix() const;
  std::string GetExtraMetricsSuffix() const;
  std::string GetRapporPrefix() const;
  std::string GetSamplingEventName() const;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPage);
};

// Factory for creating SafeBrowsingBlockingPage.  Useful for tests.
class SafeBrowsingBlockingPageFactory {
 public:
  virtual ~SafeBrowsingBlockingPageFactory() {}

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* ui_manager,
      web::WebState* web_state,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) = 0;
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
