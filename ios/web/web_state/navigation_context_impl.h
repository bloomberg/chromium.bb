// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_NAVIGATION_CONTEXT_IMPL_H_
#define IOS_WEB_WEB_STATE_NAVIGATION_CONTEXT_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "ios/web/public/web_state/navigation_context.h"
#include "url/gurl.h"

namespace web {

// Tracks information related to a single navigation.
class NavigationContextImpl : public NavigationContext {
 public:
  // Creates navigation context for sucessful navigation to a different page.
  static std::unique_ptr<NavigationContext> CreateNavigationContext(
      WebState* web_state,
      const GURL& url);

  // Creates navigation context for sucessful same page navigation.
  static std::unique_ptr<NavigationContext> CreateSamePageNavigationContext(
      WebState* web_state,
      const GURL& url);

  // Creates navigation context for the error page navigation.
  static std::unique_ptr<NavigationContext> CreateErrorPageNavigationContext(
      WebState* web_state,
      const GURL& url);

  // NavigationContext overrides:
  WebState* GetWebState() override;
  const GURL& GetUrl() const override;
  bool IsSamePage() const override;
  bool IsErrorPage() const override;

 private:
  NavigationContextImpl(WebState* web_state,
                        const GURL& url,
                        bool is_same_page,
                        bool is_error_page);
  ~NavigationContextImpl() override;

  WebState* web_state_ = nullptr;
  GURL url_;
  bool is_same_page_ = false;
  bool is_error_page_ = false;

  DISALLOW_COPY_AND_ASSIGN(NavigationContextImpl);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_NAVIGATION_CONTEXT_IMPL_H_
