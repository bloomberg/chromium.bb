// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_IOS_H_
#define COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_IOS_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/ios/favicon_web_state_dispatcher.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "url/gurl.h"

namespace dom_distiller {

class DistillerWebStateObserver;

// Loads URLs and injects JavaScript into a page, extracting the distilled page
// content.
class DistillerPageIOS : public DistillerPage {
 public:
  explicit DistillerPageIOS(FaviconWebStateDispatcher* web_state_dispatcher);
  ~DistillerPageIOS() override;

 protected:
  bool StringifyOutput() override;
  void DistillPageImpl(const GURL& url, const std::string& script) override;

 private:
  friend class DistillerWebStateObserver;

  // Called by |web_state_observer_| once the page has finished loading.
  void OnLoadURLDone(web::PageLoadCompletionStatus load_completion_status);

  // Called once the |script_| has been evaluated on the page.
  void HandleJavaScriptResult(id result);

  // Converts result of WKWebView script evaluation to base::Value
  std::unique_ptr<base::Value> ValueResultFromScriptResult(id wk_result);

  GURL url_;
  std::string script_;
  web::WebState* web_state_;
  FaviconWebStateDispatcher* web_state_dispatcher_;
  std::unique_ptr<DistillerWebStateObserver> web_state_observer_;
  base::WeakPtrFactory<DistillerPageIOS> weak_ptr_factory_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_IOS_H_
