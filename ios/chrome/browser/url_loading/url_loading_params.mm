// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UrlLoadParams* UrlLoadParams::InCurrentTab(
    const web::NavigationManager::WebLoadParams& web_params,
    WindowOpenDisposition disposition) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = disposition;
  params->web_params = web_params;
  return params;
}

UrlLoadParams* UrlLoadParams::InCurrentTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  return InCurrentTab(web_params, WindowOpenDisposition::CURRENT_TAB);
}

UrlLoadParams* UrlLoadParams::InCurrentTab(const GURL& url) {
  return InCurrentTab(web::NavigationManager::WebLoadParams(url));
}

UrlLoadParams* UrlLoadParams::InNewTab(
    const web::NavigationManager::WebLoadParams& web_params,
    bool in_incognito,
    bool in_background,
    OpenPosition append_to) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = in_background
                            ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                            : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params->web_params = web_params;
  params->in_incognito = in_incognito;
  params->append_to = append_to;
  params->user_initiated = true;
  return params;
}

UrlLoadParams* UrlLoadParams::InNewTab(const GURL& url,
                                       const GURL& virtual_url,
                                       const web::Referrer& referrer,
                                       bool in_incognito,
                                       bool in_background,
                                       OpenPosition append_to) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = in_background
                            ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                            : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params->web_params = web::NavigationManager::WebLoadParams(url);
  params->web_params.virtual_url = virtual_url;
  params->web_params.referrer = referrer;
  params->in_incognito = in_incognito;
  params->append_to = append_to;
  params->user_initiated = true;
  return params;
}

UrlLoadParams* UrlLoadParams::InNewTab(const GURL& url,
                                       const web::Referrer& referrer,
                                       bool in_incognito,
                                       bool in_background,
                                       OpenPosition append_to) {
  return InNewTab(url, GURL::EmptyGURL(), referrer, in_incognito, in_background,
                  append_to);
}

UrlLoadParams* UrlLoadParams::InNewTab(const GURL& url,
                                       bool in_incognito,
                                       bool in_background,
                                       OpenPosition append_to) {
  return InNewTab(url, web::Referrer(), in_incognito, in_background, append_to);
}

UrlLoadParams* UrlLoadParams::InNewEmptyTab(bool in_incognito,
                                            bool in_background) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = in_background
                            ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                            : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params->in_incognito = in_incognito;
  params->user_initiated = true;
  return params;
}

UrlLoadParams* UrlLoadParams::InNewFromChromeTab(const GURL& url) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params->web_params = web::NavigationManager::WebLoadParams(url);
  params->append_to = kLastTab;
  params->from_chrome = true;
  return params;
}

UrlLoadParams* UrlLoadParams::InNewForegroundTab(bool in_incognito,
                                                 CGPoint origin_point) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params->origin_point = origin_point;
  params->user_initiated = true;
  return params;
}

UrlLoadParams* UrlLoadParams::InNewForegroundTab(bool in_incognito) {
  UrlLoadParams* params = new UrlLoadParams();
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params->user_initiated = true;
  return params;
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
      user_initiated(false),
      should_focus_omnibox(false) {}

UrlLoadParams::UrlLoadParams(const UrlLoadParams& other)
    : web_params(other.web_params), disposition(other.disposition) {}

UrlLoadParams& UrlLoadParams::operator=(const UrlLoadParams& other) {
  web_params = other.web_params;
  disposition = other.disposition;

  return *this;
}
