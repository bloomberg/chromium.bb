// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_UMA_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_UMA_POLICY_H_

#include <map>
#include <string>

#include "chrome/browser/extensions/activity_log/activity_log_policy.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "url/gurl.h"

namespace extensions {

// The UmaPolicy keeps track of how many extensions have read from or modified
// a given pageload. UmaPolicy records to a histogram when a given tab is
// closed. Caveats:
//   * If multiple tabs are open for the same URL at the same time, UmaPolicy
//     treats them as if they are the same.
//   * UmaPolicy does not record statistics for incognito tabs. (For privacy.)
//   * If the number of tabs open exceeds 50, UmaPolicy stops recording stats
//     for tabs 51+. (For memory.)
//   * UmaPolicy only handles top frames; stats are not recorded for iframes.
class UmaPolicy : public ActivityLogPolicy,
                  public TabStripModelObserver,
                  public chrome::BrowserListObserver {
 public:
  // The possible status bits for a pageload. If you alter this, make sure to
  // also update GetHistogramName.
  enum PageStatus {
    NONE = 0,
    CONTENT_SCRIPT = 1,
    READ_DOM,
    MODIFIED_DOM,
    DOM_METHOD,
    DOCUMENT_WRITE,
    INNER_HTML,
    CREATED_SCRIPT,
    CREATED_IFRAME,
    CREATED_DIV,
    CREATED_LINK,
    CREATED_INPUT,
    CREATED_EMBED,
    CREATED_OBJECT,
    AD_INJECTED,
    AD_REMOVED,
    AD_REPLACED,
    AD_LIKELY_INJECTED,
    AD_LIKELY_REPLACED,
    MAX_STATUS  // Insert new page statuses right before this one.
  };

  explicit UmaPolicy(Profile* profile);

  // ActivityLogPolicy implementation.
  virtual void ProcessAction(scoped_refptr<Action> action) OVERRIDE;
  virtual void Close() OVERRIDE;

  // Gets the histogram name associated with each PageStatus.
  static const char* GetHistogramName(PageStatus status);

 protected:
  // Run when Close() is called.
  virtual ~UmaPolicy();

 private:
  // Used as a special key in the ExtensionMap.
  static const char kNumberOfTabs[];

  // The max number of tabs we track at a time.
  static const size_t kMaxTabsTracked;

  typedef std::map<std::string, int> ExtensionMap;
  typedef std::map<std::string, ExtensionMap> SiteMap;

  // BrowserListObserver
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // TabStripModelObserver
  // Fired when a page loads, either as a new tab or replacing the contents of
  // an older tab.
  virtual void TabChangedAt(content::WebContents* contents,
                            int index,
                            TabChangeType change_type) OVERRIDE;
  // Fired when a tab closes.
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            content::WebContents* contents,
                            int index) OVERRIDE;

  // Assign a status bitmask based on the action's properties.
  int MatchActionToStatus(scoped_refptr<Action> action);

  // When a page is opened, add it to the SiteMap url_status_.
  void SetupOpenedPage(const std::string& url);

  // When a page is closing, remove it from the SiteMap url_status_.
  void CleanupClosedPage(const std::string& cleaned_url,
                         content::WebContents* web_contents);

  // When a page is closing, save statistics about the page to histograms.
  void HistogramOnClose(const std::string& cleaned_url,
                        content::WebContents* web_contents);

  // Standardizes the way URLs are treated.
  static std::string CleanURL(const GURL& gurl);

  // Used by UmaPolicyTest.ProcessActionTest.
  SiteMap url_status() { return url_status_; }

  Profile* profile_;

  // URL -> extension id -> page status.
  SiteMap url_status_;

  // tab index -> URL.
  std::map<int32, std::string> tab_list_;

  FRIEND_TEST_ALL_PREFIXES(UmaPolicyTest, CleanURLTest);
  FRIEND_TEST_ALL_PREFIXES(UmaPolicyTest, MatchActionToStatusTest);
  FRIEND_TEST_ALL_PREFIXES(UmaPolicyTest, ProcessActionTest);
  FRIEND_TEST_ALL_PREFIXES(UmaPolicyTest, SiteUrlTest);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_UMA_POLICY_H_
