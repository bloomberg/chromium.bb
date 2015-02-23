// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_OPTIONS_H_
#define NET_COOKIES_COOKIE_OPTIONS_H_

#include "url/gurl.h"

namespace net {

class CookieOptions {
 public:
  // Default is to exclude httponly completely, and exclude first-party from
  // being read, which means:
  // - reading operations will not return httponly or first-party cookies.
  // - writing operations will not write httponly cookies (first-party will be
  // written).
  //
  // If a first-party URL is set, then first-party cookies which match that URL
  // will be returned.
  CookieOptions()
      : exclude_httponly_(true),
        include_first_party_only_(false),
        server_time_() {}

  void set_exclude_httponly() { exclude_httponly_ = true; }
  void set_include_httponly() { exclude_httponly_ = false; }
  bool exclude_httponly() const { return exclude_httponly_; }

  void set_include_first_party_only() { include_first_party_only_ = true; }
  bool include_first_party_only() const { return include_first_party_only_; }

  void set_first_party_url(const GURL& url) { first_party_url_ = url; }
  GURL first_party_url() const { return first_party_url_; }

  // |server_time| indicates what the server sending us the Cookie thought the
  // current time was when the cookie was produced.  This is used to adjust for
  // clock skew between server and host.
  void set_server_time(const base::Time& server_time) {
    server_time_ = server_time;
  }
  bool has_server_time() const { return !server_time_.is_null(); }
  base::Time server_time() const { return server_time_; }

 private:
  bool exclude_httponly_;
  bool include_first_party_only_;
  GURL first_party_url_;
  base::Time server_time_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_OPTIONS_H_
