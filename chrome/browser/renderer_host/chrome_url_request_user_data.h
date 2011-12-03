// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_URL_REQUEST_USER_DATA_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_URL_REQUEST_USER_DATA_H_
#pragma once

#include "net/url_request/url_request.h"

class ChromeURLRequestUserData : public net::URLRequest::UserData {
 public:
  bool is_prerender() const { return is_prerender_; }
  void set_is_prerender(bool is_prerender) { is_prerender_ = is_prerender; }

  // Creates a new ChromeURLRequestUserData instance and attaches it
  // to |request|. |request| must not have an existing ChromeURLRequestUserData
  // instance attached to it, and must be non-NULL. The returned instance
  // is owned by |request|.
  static ChromeURLRequestUserData* Create(net::URLRequest* request);

  // Delete the ChromeURLRequestUserData from a |request|. |request| must be
  // non-NULL.
  static void Delete(net::URLRequest* request);

  // Gets the ChromeURLRequestUserData instance attached to |request|, or
  // returns NULL if one is not attached. |request| must be non-NULL.
  static ChromeURLRequestUserData* Get(const net::URLRequest* request);

 private:
  ChromeURLRequestUserData();

  bool is_prerender_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLRequestUserData);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_URL_REQUEST_USER_DATA_H_
