// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_NAVIGATION_CONTEXT_IMPL_H_
#define IOS_WEB_WEB_STATE_NAVIGATION_CONTEXT_IMPL_H_

#import <WebKit/WebKit.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#import "ios/web/public/web_state/navigation_context.h"
#include "url/gurl.h"

namespace web {

// Tracks information related to a single navigation.
class NavigationContextImpl : public NavigationContext {
 public:
  // Creates navigation context for successful navigation to a different page.
  // Response headers will ne null.
  static std::unique_ptr<NavigationContextImpl> CreateNavigationContext(
      WebState* web_state,
      const GURL& url,
      ui::PageTransition page_transition,
      bool is_renderer_initiated);

#ifndef NDEBUG
  // Returns human readable description of this object.
  NSString* GetDescription() const;
#endif  // NDEBUG

  // NavigationContext overrides:
  WebState* GetWebState() override;
  const GURL& GetUrl() const override;
  ui::PageTransition GetPageTransition() const override;
  bool IsSameDocument() const override;
  bool IsPost() const override;
  NSError* GetError() const override;
  net::HttpResponseHeaders* GetResponseHeaders() const override;
  bool IsRendererInitiated() const override;
  ~NavigationContextImpl() override;

  // Setters for navigation context data members.
  void SetIsSameDocument(bool is_same_document);
  void SetIsPost(bool is_post);
  void SetError(NSError* error);
  void SetResponseHeaders(
      const scoped_refptr<net::HttpResponseHeaders>& response_headers);
  void SetIsRendererInitiated(bool is_renderer_initiated);

  // Optional unique id of the navigation item associated with this navigaiton.
  int GetNavigationItemUniqueID() const;
  void SetNavigationItemUniqueID(int unique_id);

  // Optional WKNavigationType of the associated navigation in WKWebView.
  void SetWKNavigationType(WKNavigationType wk_navigation_type);
  WKNavigationType GetWKNavigationType() const;

 private:
  NavigationContextImpl(WebState* web_state,
                        const GURL& url,
                        ui::PageTransition page_transition,
                        bool is_renderer_initiated);

  WebState* web_state_ = nullptr;
  GURL url_;
  const ui::PageTransition page_transition_;
  bool is_same_document_ = false;
  bool is_post_ = false;
  base::scoped_nsobject<NSError> error_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool is_renderer_initiated_ = false;
  int navigation_item_unique_id_ = -1;
  WKNavigationType wk_navigation_type_ = WKNavigationTypeOther;

  DISALLOW_COPY_AND_ASSIGN(NavigationContextImpl);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_NAVIGATION_CONTEXT_IMPL_H_
