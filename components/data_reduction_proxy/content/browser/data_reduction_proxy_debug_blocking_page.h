// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_DATA_REDUCTION_PROXY_BLOCKING_PAGE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_DATA_REDUCTION_PROXY_BLOCKING_PAGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class InterstitialPage;
class WebContents;
}

namespace data_reduction_proxy {

class DataReductionProxyDebugBlockingPageFactory;
class DataReductionProxyDebugUIManager;

// When a user is about to visit a page and the Data Reduction Proxy bypasses
// this class, show an interstitial page with some options (go back, continue)
// to alert the user of the bypass.
//
// The DataReductionProxyDebugBlockingPage is created by the
// DataReductionProxyDebugUIManager on the UI thread when it has been determined
// that a page will be bypassed. The operation of the blocking page occurs on
// the UI thread, where it waits for the user to make a decision about what to
// do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// DataReductionProxyDebugUIManager so that the request can be canceled for the
// new page, or allowed to continue.
//
// A web page may contain several resources flagged as blacklisted. This
// only results in one interstitial being shown. If the user decides to proceed
// in the first interstitial, another interstitial will not be displayed for a
// predetermined amount of time.
class DataReductionProxyDebugBlockingPage
    : public content::InterstitialPageDelegate {
 public:
  typedef DataReductionProxyDebugUIManager::BypassResource BypassResource;
  typedef std::vector<BypassResource> BypassResourceList;
  typedef std::map<content::WebContents*, BypassResourceList> BypassResourceMap;

  ~DataReductionProxyDebugBlockingPage() override;

  // Interstitial type for testing.
  static content::InterstitialPageDelegate::TypeID kTypeForTesting;

  // Creates a blocking page. Use ShowBlockingPage when access to the blocking
  // page directly isn't needed.
  static DataReductionProxyDebugBlockingPage* CreateBlockingPage(
      DataReductionProxyDebugUIManager* ui_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      content::WebContents* web_contents,
      const BypassResource& bypass_resource,
      const std::string& app_locale);

  // Shows a blocking page warning about a Data Reduction Proxy bypass for a
  // specific resource. This method can be called several times, if an
  // interstitial is already showing, the new one will be queued and displayed
  // if the user decides to proceed on the currently showing interstitial.
  static void ShowBlockingPage(
      DataReductionProxyDebugUIManager* ui_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const BypassResource& bypass_resource,
      const std::string& app_locale);

  content::InterstitialPage* interstitial_page() const;

  // Makes |factory| the factory used to instantiate
  // DataReductionProxyDebugBlockingPage objects. Useful for tests.
  static void RegisterFactory(
      DataReductionProxyDebugBlockingPageFactory* factory) {
    factory_ = factory;
  }

  // InterstitialPageDelegate overrides:
  std::string GetHTMLContents() override;
  void OnProceed() override;
  void OnDontProceed() override;
  void CommandReceived(const std::string& command) override;
  content::InterstitialPageDelegate::TypeID GetTypeForTesting() const override;

 protected:
  friend class DataReductionProxyDebugBlockingPageFactoryImpl;

  enum BlockingPageEvent {
    SHOW,
    PROCEED,
    DONT_PROCEED,
  };

  // This constructor should not be called directly. Use ShowBlockingPage
  // instead.
  DataReductionProxyDebugBlockingPage(
      DataReductionProxyDebugUIManager* ui_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      content::WebContents* web_contents,
      const BypassResourceList& resource_list,
      const std::string& app_locale);

  // Creates the InterstitialPage and shows it.
  void Show();

  // A list of DataReductionProxyDebugUIManager::BypassResource for a tab that
  // the user should be warned about. They are queued when displaying more than
  // one interstitial at a time.
  static BypassResourceMap* GetBypassResourcesMap();

  // Prevents creating the actual interstitial view for testing.
  void DontCreateViewForTesting();

  // Notifies the DataReductionProxyDebugUIManager on the IO thread whether to
  // proceed or not for the |resources|.
  static void NotifyDataReductionProxyDebugUIManager(
      DataReductionProxyDebugUIManager* ui_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const BypassResourceList& resource_list, bool proceed);

  // Returns true if the passed |bypass_resources| is blocking the load of
  // the main page.
  static bool IsMainPageLoadBlocked(const BypassResourceList& resource_list);

  // For reporting back user actions.
  DataReductionProxyDebugUIManager* ui_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // True if the interstitial is blocking the main page because it is on one
  // of our lists. False if a subresource is being blocked, or in the case of
  // client-side detection where the interstitial is shown after page load
  // finishes.
  bool is_main_frame_load_blocked_;

  // The index of a navigation entry that should be removed when DontProceed()
  // is invoked, -1 if not entry should be removed.
  int navigation_entry_index_to_remove_;

  // The list of bypassed resources this page is warning about.
  BypassResourceList resource_list_;

  // Records if the user pressed the proceed button.
  bool proceeded_;

  // The WebContents for this interstitial.
  content::WebContents* web_contents_;

  // The url of the bypassed resource.
  GURL url_;

  // Owns this DataReductionproxyDebugBlockingPage.
  content::InterstitialPage* interstitial_page_;

  // The factory used to instantiate DataReductionProxyDebugBlockingPage
  // objects. Useful for tests, so they can provide their own implementation of
  // DataReductionProxyDebugBlockingPage.
  static DataReductionProxyDebugBlockingPageFactory* factory_;

 private:
  // Used by tests so that the interstitial does not create a view.
  bool create_view_;

  // The application locale. Used to set the language of the blocking page.
  const std::string& app_locale_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyDebugBlockingPage);
};

// Factory for creating DataReductionProxyDebugBlockingPage. Useful for tests.
class DataReductionProxyDebugBlockingPageFactory {
 public:
  virtual ~DataReductionProxyDebugBlockingPageFactory() { }

  virtual DataReductionProxyDebugBlockingPage* CreateDataReductionProxyPage(
      DataReductionProxyDebugUIManager* ui_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      content::WebContents* web_contents,
      const DataReductionProxyDebugBlockingPage::BypassResourceList&
          resource_list,
      const std::string& app_locale) = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_DATA_REDUCTION_PROXY_BLOCKING_PAGE_H_
