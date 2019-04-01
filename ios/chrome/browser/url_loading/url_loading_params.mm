// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UrlLoadParams* UrlLoadParams::InCurrentTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = WindowOpenDisposition::CURRENT_TAB;
  params->web_params = web_params;
  return params;
}

UrlLoadParams* UrlLoadParams::InCurrentTab(const GURL& url,
                                           const GURL& virtual_url) {
  web::NavigationManager::WebLoadParams web_params(url);
  web_params.virtual_url = virtual_url;
  return InCurrentTab(web_params);
}

UrlLoadParams* UrlLoadParams::InCurrentTab(const GURL& url) {
  return InCurrentTab(web::NavigationManager::WebLoadParams(url));
}

UrlLoadParams* UrlLoadParams::InNewTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams* params = new UrlLoadParams();
  params->web_params = web_params;
  return params;
}

UrlLoadParams* UrlLoadParams::InNewTab(const GURL& url,
                                       const GURL& virtual_url) {
  web::NavigationManager::WebLoadParams web_params(url);
  web_params.virtual_url = virtual_url;
  return InNewTab(web_params);
}

UrlLoadParams* UrlLoadParams::InNewTab(const GURL& url) {
  return InNewTab(web::NavigationManager::WebLoadParams(url));
}

UrlLoadParams* UrlLoadParams::SwitchToTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  params->web_params = web_params;
  return params;
}

UrlLoadParams::UrlLoadParams()
    : web_params(GURL()),
      disposition(WindowOpenDisposition::NEW_FOREGROUND_TAB),
      in_incognito(false),
      append_to(kLastTab),
      origin_point(CGPointZero),
      from_chrome(false),
      user_initiated(true),
      should_focus_omnibox(false) {}

UrlLoadParams::UrlLoadParams(const UrlLoadParams& other)
    : web_params(other.web_params), disposition(other.disposition) {}

UrlLoadParams& UrlLoadParams::operator=(const UrlLoadParams& other) {
  web_params = other.web_params;
  disposition = other.disposition;

  return *this;
}

UrlLoadParams* UrlLoadParams::Transition(ui::PageTransition transition) {
  this->web_params.transition_type = transition;
  return this;
}

UrlLoadParams* UrlLoadParams::InIncognito(bool in_incognito) {
  this->in_incognito = in_incognito;
  return this;
}

UrlLoadParams* UrlLoadParams::Referrer(const web::Referrer& referrer) {
  this->web_params.referrer = referrer;
  return this;
}

UrlLoadParams* UrlLoadParams::InBackground(bool in_background) {
  this->disposition = in_background ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                                    : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  return this;
}

UrlLoadParams* UrlLoadParams::AppendTo(OpenPosition append_to) {
  this->append_to = append_to;
  return this;
}

UrlLoadParams* UrlLoadParams::OriginPoint(CGPoint origin_point) {
  this->origin_point = origin_point;
  return this;
}

UrlLoadParams* UrlLoadParams::FromChrome(bool from_chrome) {
  this->from_chrome = from_chrome;
  return this;
}

UrlLoadParams* UrlLoadParams::UserInitiated(bool user_initiated) {
  this->user_initiated = user_initiated;
  return this;
}

UrlLoadParams* UrlLoadParams::ShouldFocusOmnibox(bool should_focus_omnibox) {
  this->should_focus_omnibox = should_focus_omnibox;
  return this;
}

UrlLoadParams* UrlLoadParams::Disposition(WindowOpenDisposition disposition) {
  this->disposition = disposition;
  return this;
}
