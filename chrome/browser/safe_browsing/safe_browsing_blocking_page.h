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

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

class MalwareDetails;
class SafeBrowsingBlockingPageFactory;

namespace base {
class DictionaryValue;
class MessageLoop;
}

namespace content {
class InterstitialPage;
class WebContents;
}

class SafeBrowsingBlockingPage : public content::InterstitialPageDelegate {
 public:
  typedef SafeBrowsingUIManager::UnsafeResource UnsafeResource;
  typedef std::vector<UnsafeResource> UnsafeResourceList;
  typedef std::map<content::WebContents*, UnsafeResourceList> UnsafeResourceMap;

  virtual ~SafeBrowsingBlockingPage();

  // Shows a blocking page warning the user about phishing/malware for a
  // specific resource.
  // You can call this method several times, if an interstitial is already
  // showing, the new one will be queued and displayed if the user decides
  // to proceed on the currently showing interstitial.
  static void ShowBlockingPage(
      SafeBrowsingUIManager* ui_manager, const UnsafeResource& resource);

  // Makes the passed |factory| the factory used to instanciate
  // SafeBrowsingBlockingPage objects. Useful for tests.
  static void RegisterFactory(SafeBrowsingBlockingPageFactory* factory) {
    factory_ = factory;
  }

  // InterstitialPageDelegate method:
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void OverrideRendererPrefs(
      content::RendererPreferences* prefs) OVERRIDE;
  virtual void OnProceed() OVERRIDE;
  virtual void OnDontProceed() OVERRIDE;

 protected:
  template <class TestSBInterstitialPage>
  friend class SafeBrowsingBlockingPageTest;
  template <class TestSBInterstitialPage>
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ProceedThenDontProceed);

  void SetReportingPreference(bool report);
  void UpdateReportingPref();  // Used for the transition from old to new pref.

  // Don't instanciate this class directly, use ShowBlockingPage instead.
  SafeBrowsingBlockingPage(SafeBrowsingUIManager* ui_manager,
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

  template <class TestSBInterstitialPage>
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
      MalwareReportsTransitionDisabled);
  template <class TestSBInterstitialPage>
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
      MalwareReportsToggling);

  enum BlockingPageEvent {
    SHOW,
    PROCEED,
    DONT_PROCEED,
    SHOW_ADVANCED,
  };

  // Records a user action for this interstitial, using the form
  // SBInterstitial[Phishing|Malware|Multiple][Show|Proceed|DontProceed].
  void RecordUserAction(BlockingPageEvent event);

  // Used to query the HistoryService to see if the URL is in history. For UMA.
  void OnGotHistoryCount(bool success, int num_visits, base::Time first_visit);

  // Records the time it took for the user to react to the
  // interstitial.  We won't double-count if this method is called
  // multiple times.
  void RecordUserReactionTime(const std::string& command);

  // Checks if we should even show the malware details option. For example, we
  // don't show it in incognito mode.
  bool CanShowMalwareDetailsOption();

  // Called when the insterstitial is going away. If there is a
  // pending malware details object, we look at the user's
  // preferences, and if the option to send malware details is
  // enabled, the report is scheduled to be sent on the |ui_manager_|.
  void FinishMalwareDetails(int64 delay_ms);

  // Returns the boolean value of the given |pref| from the PrefService of the
  // Profile associated with |web_contents_|.
  bool IsPrefEnabled(const char* pref);

  // A list of SafeBrowsingUIManager::UnsafeResource for a tab that the user
  // should be warned about.  They are queued when displaying more than one
  // interstitial at a time.
  static UnsafeResourceMap* GetUnsafeResourcesMap();

  // Notifies the SafeBrowsingUIManager on the IO thread whether to proceed
  // or not for the |resources|.
  static void NotifySafeBrowsingUIManager(
      SafeBrowsingUIManager* ui_manager,
      const UnsafeResourceList& resources, bool proceed);

  // Returns true if the passed |unsafe_resources| is blocking the load of
  // the main page.
  static bool IsMainPageLoadBlocked(
      const UnsafeResourceList& unsafe_resources);

  friend class SafeBrowsingBlockingPageFactoryImpl;

  // For reporting back user actions.
  SafeBrowsingUIManager* ui_manager_;
  base::MessageLoop* report_loop_;

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

  // Time when the interstitial was show.  This variable is set in
  // GetHTMLContents() which is called right before the interstitial
  // is shown to the user. Will return is_null() once we reported the
  // user action.
  base::TimeTicks interstitial_show_time_;

  // Whether the user has expanded the "see more" section of the page already
  // during this interstitial page.
  bool has_expanded_see_more_section_;

  // Whether the user has left the reporting checkbox checked.
  bool reporting_checkbox_checked_;

  // Which type of interstitial this is.
  enum {
    TYPE_MALWARE,
    TYPE_PHISHING,
    TYPE_MALWARE_AND_PHISHING,
  } interstitial_type_;

  // The factory used to instanciate SafeBrowsingBlockingPage objects.
  // Usefull for tests, so they can provide their own implementation of
  // SafeBrowsingBlockingPage.
  static SafeBrowsingBlockingPageFactory* factory_;

  // How many times is this same URL in history? Used for histogramming.
  int num_visits_;
  base::CancelableTaskTracker request_tracker_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPage);
};

class SafeBrowsingBlockingPageV2 : public SafeBrowsingBlockingPage {
 public:
  // Don't instanciate this class directly, use ShowBlockingPage instead.
  SafeBrowsingBlockingPageV2(SafeBrowsingUIManager* ui_manager,
                             content::WebContents* web_contents,
                             const UnsafeResourceList& unsafe_resources);

  // InterstitialPageDelegate method:
  virtual std::string GetHTMLContents() OVERRIDE;

 private:
  // Fills the passed dictionary with the strings passed to JS Template when
  // creating the HTML.
  void PopulateMultipleThreatStringDictionary(base::DictionaryValue* strings);
  void PopulateMalwareStringDictionary(base::DictionaryValue* strings);
  void PopulatePhishingStringDictionary(base::DictionaryValue* strings);

  // A helper method used by the Populate methods above used to populate common
  // fields.
  void PopulateStringDictionary(base::DictionaryValue* strings,
                                const base::string16& title,
                                const base::string16& headline,
                                const base::string16& description1,
                                const base::string16& description2,
                                const base::string16& description3);

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageV2);
};

class SafeBrowsingBlockingPageV3 : public SafeBrowsingBlockingPage {
 public:
  SafeBrowsingBlockingPageV3(SafeBrowsingUIManager* ui_manager,
                             content::WebContents* web_contents,
                             const UnsafeResourceList& unsafe_resources);

  // InterstitialPageDelegate method:
  virtual std::string GetHTMLContents() OVERRIDE;

 private:
  // Fills the passed dictionary with the values to be passed to the template
  // when creating the HTML.
  void PopulateMalwareLoadTimeData(base::DictionaryValue* load_time_data);
  void PopulatePhishingLoadTimeData(base::DictionaryValue* load_time_data);

  // For the M37 FieldTrial: this contains the name of the condition.
  std::string trial_condition_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPageV3);
};

// Factory for creating SafeBrowsingBlockingPage.  Useful for tests.
class SafeBrowsingBlockingPageFactory {
 public:
  virtual ~SafeBrowsingBlockingPageFactory() { }

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) = 0;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
