// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_REQUEST_STAGES_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_REQUEST_STAGES_H_
#pragma once

namespace extensions {

// The stages of the web request during which a condition can be tested and
// an action can be applied. This is required because for example the response
// headers cannot be tested before a request has been sent.
//
// TODO(battre) rename to singular.
enum RequestStages {
  ON_BEFORE_REQUEST = 1 << 0,
  ON_BEFORE_SEND_HEADERS = 1 << 1,
  ON_SEND_HEADERS = 1 << 2,
  ON_HEADERS_RECEIVED = 1 << 3,
  ON_AUTH_REQUIRED = 1 << 4,
  ON_BEFORE_REDIRECT = 1 << 5,
  ON_RESPONSE_STARTED = 1 << 6,
  ON_COMPLETED = 1 << 7,
  ON_ERROR = 1 << 8
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_WEBREQUEST_REQUEST_STAGES_H_
