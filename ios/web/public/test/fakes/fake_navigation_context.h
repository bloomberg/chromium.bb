// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

namespace web {

// Tracks information related to a single navigation.
class FakeNavigationContext : public NavigationContext {
 public:
  ~FakeNavigationContext() override;
  FakeNavigationContext();

  // NavigationContext overrides:
  WebState* GetWebState() override;
  const GURL& GetUrl() const override;
  ui::PageTransition GetPageTransition() const override;
  bool IsSameDocument() const override;
  bool IsPost() const override;
  NSError* GetError() const override;
  net::HttpResponseHeaders* GetResponseHeaders() const override;
  bool IsRendererInitiated() const override;

  // Setters for navigation context data members.
  void SetWebState(std::unique_ptr<WebState> web_state);
  void SetUrl(const GURL& url);
  void SetPageTransition(ui::PageTransition transition);
  void SetIsSameDocument(bool same_document);
  void SetIsPost(bool is_post);
  void SetError(NSError* error);
  void SetResponseHeaders(
      const scoped_refptr<net::HttpResponseHeaders>& response_headers);
  void SetIsRendererInitiated(bool renderer_initiated);

 private:
  std::unique_ptr<WebState> web_state_;
  GURL url_;
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;
  bool same_document_ = false;
  bool is_post_ = false;
  __strong NSError* error_ = nil;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool renderer_initiated_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationContext);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_
