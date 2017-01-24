// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DISTILLER_PAGE_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DISTILLER_PAGE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/ios/distiller_page_ios.h"
#include "url/gurl.h"

namespace web {
class BrowserState;
}

namespace reading_list {

class FaviconWebStateDispatcher;

// An DistillerPageIOS that will retain WebState to allow favicon download and
// and add a 2 seconds delay between loading and distillation.
class ReadingListDistillerPage : public dom_distiller::DistillerPageIOS {
 public:
  typedef base::Callback<void(const GURL&, const GURL&)> RedirectionCallback;

  explicit ReadingListDistillerPage(
      web::BrowserState* browser_state,
      FaviconWebStateDispatcher* web_state_dispatcher);
  ~ReadingListDistillerPage() override;

  // Sets a callback that will be called just before distillation happen with
  // the URL of the page that will effectively be distilled.
  void SetRedirectionCallback(RedirectionCallback redirection_callback);

 protected:
  void DistillPageImpl(const GURL& url, const std::string& script) override;
  void OnDistillationDone(const GURL& page_url,
                          const base::Value* value) override;
  void OnLoadURLDone(
      web::PageLoadCompletionStatus load_completion_status) override;

 private:
  // Returns wether there is the loading has no error and if the distillation
  // can continue.
  bool IsLoadingSuccess(web::PageLoadCompletionStatus load_completion_status);
  // Work around the fact that articles opened from Google Search page and
  // Google News are presented in an iframe. This method detects if the current
  // page is a Google AMP and navigate to the iframe in that case.
  // Returns whether the current page is a Google AMP page.
  // IsGoogleCachedAMPPage will determine if the current page is a Google AMP
  // page.
  bool IsGoogleCachedAMPPage();
  // HandleGoogleCachedAMPPage will navigate to the iframe containing the actual
  // article page.
  void HandleGoogleCachedAMPPage();
  // Handles the JavaScript response. If the URL of the iframe is returned,
  // triggers a navigation to it. Stop distillation of the page there as the new
  // load will trigger a new distillation.
  bool HandleGoogleCachedAMPPageJavaScriptResult(id result, id error);
  // Continue the distillation on the page that is currently loaded in
  // |CurrentWebState()|.
  void ContinuePageDistillation();

  // Continues distillation by calling superclass |OnLoadURLDone|.
  void DelayedOnLoadURLDone();

  RedirectionCallback redirection_callback_;
  GURL original_url_;

  FaviconWebStateDispatcher* web_state_dispatcher_;
  base::WeakPtrFactory<ReadingListDistillerPage> weak_ptr_factory_;
};

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DISTILLER_PAGE_H_
