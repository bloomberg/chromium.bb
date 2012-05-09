// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
// The SafeBrowsingBlockingPage is created by the SafeBrowsingService on the UI
// thread when we've determined that a page is malicious. The operation of the
// blocking page occurs on the UI thread, where it waits for the user to make a
// decision about what to do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// SafeBrowsingService so that we can cancel the request for the new page, or
// or allow it to continue.
//
// A web page may contain several resources flagged as malware/phishing.  This
// results into more than one interstitial being shown.  On the first unsafe
// resource received we show an interstitial.  Any subsequent unsafe resource
// notifications while the first interstitial is showing is queued.  If the user
// decides to proceed in the first interstitial, we display all queued unsafe
// resources in a new interstitial.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "googleurl/src/gurl.h"

class MalwareDetails;
class MessageLoop;
class SafeBrowsingBlockingPageFactory;

namespace base {
class DictionaryValue;
}

namespace content {
class InterstitialPage;
class WebContents;
}

class SafeBrowsingBlockingPage : public content::InterstitialPageDelegate {
 public:
  typedef std::vector<SafeBrowsingService::UnsafeResource> UnsafeResourceList;
  typedef std::map<content::WebContents*, UnsafeResourceList> UnsafeResourceMap;

  virtual ~SafeBrowsingBlockingPage();

  // Shows a blocking page warning the user about phishing/malware for a
  // specific resource.
  // You can call this method several times, if an interstitial is already
  // showing, the new one will be queued and displayed if the user decides
  // to proceed on the currently showing interstitial.
  static void ShowBlockingPage(
      SafeBrowsingService* service,
      const SafeBrowsingService::UnsafeResource& resource);

  // Makes the passed |factory| the factory used to instanciate
  // SafeBrowsingBlockingPage objects. Usefull for tests.
  static void RegisterFactory(SafeBrowsingBlockingPageFactory* factory) {
    factory_ = factory;
  }

  // InterstitialPageDelegate method:
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void OverrideRendererPrefs(
      content::RendererPreferences* prefs) OVERRIDE;
  virtual void OnProceed() OVERRIDE;
  virtual void OnDontProceed() OVERRIDE;

 protected:
  friend class SafeBrowsingBlockingPageTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ProceedThenDontProceed);

  void SetReportingPreference(bool report);

  // Don't instanciate this class directly, use ShowBlockingPage instead.
  SafeBrowsingBlockingPage(SafeBrowsingService* service,
                           content::WebContents* web_contents,
                           const UnsafeResourceList& unsafe_resources);

  // After a malware interstitial where the user opted-in to the
  // report but clicked "proceed anyway", we delay the call to
  // MalwareDetails::FinishCollection() by this much time (in
  // milliseconds), in order to get data from the blocked resource itself.
  int64 malware_details_proceed_delay_ms_;
  content::InterstitialPage* interstitial_page() const {
    return interstitial_page_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest, MalwareReports);

  enum BlockingPageEvent {
    SHOW,
    PROCEED,
    DONT_PROCEED,
  };

  // Fills the passed dictionary with the strings passed to JS Template when
  // creating the HTML.
  void PopulateMultipleThreatStringDictionary(base::DictionaryValue* strings);
  void PopulateMalwareStringDictionary(base::DictionaryValue* strings);
  void PopulatePhishingStringDictionary(base::DictionaryValue* strings);

  // A helper method used by the Populate methods above used to populate common
  // fields.
  void PopulateStringDictionary(base::DictionaryValue* strings,
                                const string16& title,
                                const string16& headline,
                                const string16& description1,
                                const string16& description2,
                                const string16& description3);

  // Records a user action for this interstitial, using the form
  // SBInterstitial[Phishing|Malware|Multiple][Show|Proceed|DontProceed].
  void RecordUserAction(BlockingPageEvent event);

  // Checks if we should even show the malware details option. For example, we
  // don't show it in incognito mode.
  bool CanShowMalwareDetailsOption();

  // Called when the insterstitial is going away. If there is a
  // pending malware details object, we look at the user's
  // preferences, and if the option to send malware details is
  // enabled, the report is scheduled to be sent on the |sb_service_|.
  void FinishMalwareDetails(int64 delay_ms);

  // A list of SafeBrowsingService::UnsafeResource for a tab that the user
  // should be warned about.  They are queued when displaying more than one
  // interstitial at a time.
  static UnsafeResourceMap* GetUnsafeResourcesMap();

  // Notifies the SafeBrowsingService on the IO thread whether to proceed or not
  // for the |resources|.
  static void NotifySafeBrowsingService(SafeBrowsingService* sb_service,
                                        const UnsafeResourceList& resources,
                                        bool proceed);

  // Returns true if the passed |unsafe_resources| is blocking the load of
  // the main page.
  static bool IsMainPageLoadBlocked(
      const UnsafeResourceList& unsafe_resources);

  friend class SafeBrowsingBlockingPageFactoryImpl;

  // For reporting back user actions.
  SafeBrowsingService* sb_service_;
  MessageLoop* report_loop_;

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

  // A MalwareDetails object that we start generating when the
  // blocking page is shown. The object will be sent when the warning
  // is gone (if the user enables the feature).
  scoped_refptr<MalwareDetails> malware_details_;

  bool proceeded_;

  content::WebContents* web_contents_;
  GURL url_;
  content::InterstitialPage* interstitial_page_;  // Owns us

  // The factory used to instanciate SafeBrowsingBlockingPage objects.
  // Usefull for tests, so they can provide their own implementation of
  // SafeBrowsingBlockingPage.
  static SafeBrowsingBlockingPageFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPage);
};

// Factory for creating SafeBrowsingBlockingPage.  Useful for tests.
class SafeBrowsingBlockingPageFactory {
 public:
  virtual ~SafeBrowsingBlockingPageFactory() { }

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingService* service,
      content::WebContents* web_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) = 0;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
