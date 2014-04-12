// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_PAGE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_PAGE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "url/gurl.h"

namespace dom_distiller {

struct DistilledPageInfo {
  std::string title;
  std::string html;
  std::string next_page_url;
  std::string prev_page_url;
  std::vector<std::string> image_urls;
  DistilledPageInfo();
  ~DistilledPageInfo();

 private:
  DISALLOW_COPY_AND_ASSIGN(DistilledPageInfo);
};

// Injects JavaScript into a page, and uses it to extract and return long-form
// content. The class can be reused to load and distill multiple pages,
// following the state transitions described along with the class's states.
class DistillerPage {
 public:
  typedef base::Callback<void(scoped_ptr<DistilledPageInfo> distilled_page,
                              bool distillation_successful)>
      DistillerPageCallback;

  DistillerPage();
  virtual ~DistillerPage();

  // Initializes a |DistillerPage|. It must be called before any
  // other functions, and must only be called once.
  void Init();

  // Loads a URL. |OnLoadURLDone| is called when the load completes or fails.
  // May be called when the distiller is idle or a page is available.
  void DistillPage(const GURL& url, const DistillerPageCallback& callback);
  virtual void OnLoadURLDone();
  virtual void OnLoadURLFailed();

  // Injects and executes JavaScript in the context of a loaded page. |LoadURL|
  // must complete before this function is called. May be called only when
  // a page is available.
  void ExecuteJavaScript();

  // Called when the JavaScript execution completes. |page_url| is the url of
  // the distilled page. |value| contains data returned by the script.
  virtual void OnExecuteJavaScriptDone(const GURL& page_url,
                                       const base::Value* value);

 protected:
  enum State {
    // No context has yet been set in which to load or distill a page.
    NO_CONTEXT,
    // The page distiller has been initialized and is idle.
    IDLE,
    // A page is currently loading.
    LOADING_PAGE,
    // A page has loaded within the specified context.
    PAGE_AVAILABLE,
    // There was an error processing the page.
    PAGELOAD_FAILED,
    // JavaScript is executing within the context of the page. When the
    // JavaScript completes, the state will be returned to |PAGE_AVAILABLE|.
    EXECUTING_JAVASCRIPT
  };

  // Called by |Init| to do plaform-specific initialization work set up an
  // environment in which a page can be loaded.
  virtual void InitImpl() = 0;

  // Called by |LoadURL| to carry out platform-specific instructions to load a
  // page.
  virtual void LoadURLImpl(const GURL& gurl) = 0;

  // Called by |ExecuteJavaScript| to carry out platform-specific instructions
  // to inject and execute JavaScript within the context of the loaded page.
  virtual void ExecuteJavaScriptImpl(const std::string& script) = 0;

  // The current state of the |DistillerPage|, initially |NO_CONTEXT|.
  State state_;

 private:
  DistillerPageCallback distiller_page_callback_;
  DISALLOW_COPY_AND_ASSIGN(DistillerPage);
};

// Factory for generating a |DistillerPage|.
class DistillerPageFactory {
 public:
  virtual ~DistillerPageFactory();

  virtual scoped_ptr<DistillerPage> CreateDistillerPage() const = 0;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_PAGE_H_
