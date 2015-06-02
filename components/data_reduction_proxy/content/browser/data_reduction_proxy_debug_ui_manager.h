// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_DEBUG_UI_MANAGER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_DEBUG_UI_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace data_reduction_proxy {

// Construction needs to happen on the main thread.
class DataReductionProxyDebugUIManager
    : public base::RefCountedThreadSafe<DataReductionProxyDebugUIManager> {
 public:
  // Passed a boolean indicating whether or not it is OK to proceed with
  // loading an URL.
  typedef base::Callback<void(bool /*proceed*/)> UrlCheckCallback;

  // Structure used to pass parameters between the IO and UI thread when
  // interacting with the blocking page.
  struct BypassResource {
    BypassResource();
    ~BypassResource();

    GURL url;
    bool is_subresource;
    UrlCheckCallback callback;  // This is called back on the IO thread.
    int render_process_host_id;
    int render_view_id;
  };

  // The DataReductionProxyDebugUIManager handles displaying blocking pages.
  // After a page is loaded from the blocking page, another blocking page will
  // not be shown for five minutes. Requires an application locale
  // (i.e. g_browser_process->GetApplicationLocale()).|app_locale| is used to
  // determine the language of the blocking page.
  DataReductionProxyDebugUIManager(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const std::string& app_locale);

  // Using |resource.render_process_host_id| and |resource.render_view_id|
  // checks if WebContents exists for the RenderViewHost retreived from these
  // id's.
  virtual bool IsTabClosed(const BypassResource& resource) const;

  // Called on the UI thread to display a blocking page. |url| is the url
  // of the resource that bypassed. If the request contained a chain of
  // redirects, |url| is the last url in the chain.
  virtual void DisplayBlockingPage(const BypassResource& resource);

  // Virtual for testing purposes.
  virtual void ShowBlockingPage(const BypassResource& resource);

  // The blocking page on the UI thread has completed.
  void OnBlockingPageDone(
      const std::vector<BypassResource>& resources, bool proceed);

 private:
  // Ref counted classes have private destructors to avoid any code deleting the
  // object accidentally while there are still references to it.
  friend class base::RefCountedThreadSafe<DataReductionProxyDebugUIManager>;
  friend class TestDataReductionProxyDebugUIManager;

  virtual ~DataReductionProxyDebugUIManager();

  // Records the last time the blocking page was shown. Once the blocking page
  // is shown, it is not displayed for a period of time.
  base::Time blocking_page_last_shown_;

  // A task runner to ensure that calls to DisplayBlockingPage take place on the
  // UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // A task runner to post OnBlockingPageDone to the IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  const std::string& app_locale_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyDebugUIManager);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_DEBUG_UI_MANAGER_H_
