// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#import "ios/web/public/web_state/navigation_context.h"

class GURL;

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

  // Setters for navigation context data members.
  void SetResponseHeaders(
      const scoped_refptr<net::HttpResponseHeaders>& response_headers);

 private:
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationContext);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_
