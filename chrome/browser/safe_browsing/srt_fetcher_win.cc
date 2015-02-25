// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_fetcher_win.h"

#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/srt_field_trial_win.h"
#include "chrome/browser/safe_browsing/srt_global_error_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using safe_browsing::GetSRTDownloadURL;

namespace {

void DisplaySRTPrompt(const base::FilePath& download_path) {
  // Find the last active browser, which may be NULL, in which case we won't
  // show the prompt this time and will wait until the next run of the
  // reporter. We can't use other ways of finding a browser because we don't
  // have a profile.
  chrome::HostDesktopType desktop_type = chrome::GetActiveDesktop();
  Browser* browser = chrome::FindLastActiveWithHostDesktopType(desktop_type);
  if (!browser)
    return;

  Profile* profile = browser->profile();
  DCHECK(profile);

  // Make sure we have a tabbed browser since we need to anchor the bubble to
  // the toolbar's wrench menu. Create one if none exist already.
  if (browser->type() != Browser::TYPE_TABBED) {
    browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
    if (!browser)
      browser = new Browser(Browser::CreateParams(profile, desktop_type));
  }
  GlobalErrorService* global_error_service =
      GlobalErrorServiceFactory::GetForProfile(profile);
  SRTGlobalError* global_error =
      new SRTGlobalError(global_error_service, download_path);

  // Ownership of |global_error| is passed to the service. The error removes
  // itself from the service and self-destructs when done.
  global_error_service->AddGlobalError(global_error);

  // Do not try to show bubble if another GlobalError is already showing
  // one. The bubble will be shown once the others have been dismissed.
  bool need_to_show_bubble = true;
  for (GlobalError* error : global_error_service->errors()) {
    if (error->GetBubbleView()) {
      need_to_show_bubble = false;
      break;
    }
  }
  if (need_to_show_bubble)
    global_error->ShowBubbleView(browser);
}

// Class that will attempt to download the SRT, showing the SRT notification
// bubble when the download operation is complete. Instances of SRTFetcher own
// themselves, they will self-delete on completion of the network request when
// OnURLFetchComplete is called.
class SRTFetcher : public net::URLFetcherDelegate {
 public:
  SRTFetcher()
      : url_fetcher_(net::URLFetcher::Create(0,
                                             GURL(GetSRTDownloadURL()),
                                             net::URLFetcher::GET,
                                             this)) {
    url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->SetMaxRetriesOn5xx(3);
    url_fetcher_->SaveResponseToTemporaryFile(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    url_fetcher_->SetRequestContext(
        g_browser_process->system_request_context());
    url_fetcher_->Start();
  }

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    // Take ownership of the fetcher in this scope (source == url_fetcher_).
    DCHECK_EQ(url_fetcher_.get(), source);

    base::FilePath download_path;
    if (source->GetStatus().is_success() &&
        source->GetResponseCode() == net::HTTP_OK) {
      if (source->GetResponseAsFilePath(true, &download_path)) {
        DCHECK(!download_path.empty());
      }
    }

    // Even if the fetch could not be completed, still display the prompt. In
    // this case, the prompt will fall back to visiting the download page.
    // TODO(mad): Consider implementing another layer of retries / alternate
    //            fetching mechanisms. http://crbug.com/460293
    // TODO(mad): In the event the browser is closed before the prompt displays,
    //            we will wait until the next scanner run to re-display it.
    //            Improve this. http://crbug.com/460295
    DisplaySRTPrompt(download_path);

    // Explicitly destroy the url_fetcher_ to avoid destruction races.
    url_fetcher_.reset();

    // At this point, the url_fetcher_ is gone and this SRTFetcher instance is
    // no longer needed.
    delete this;
  }

 private:
  ~SRTFetcher() override {}

  // The underlying URL fetcher. The instance is alive from construction through
  // OnURLFetchComplete.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SRTFetcher);
};

}  // namespace

namespace safe_browsing {

void FetchSRTAndDisplayBubble() {
  // All the work happens in the self-deleting class above.
  new SRTFetcher();
}

}  // namespace safe_browsing
