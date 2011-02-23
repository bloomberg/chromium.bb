// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_H_
#pragma once

#include <string>

#include "base/values.h"
#include "googleurl/src/gurl.h"

// A single tuple of (protocol, url, title) that indicates how URLs of the
// given protocol should be rewritten to be handled.

class ProtocolHandler {
 public:
  static ProtocolHandler* CreateProtocolHandler(const std::string& protocol,
                                                const GURL& url,
                                                const string16& title);
  static ProtocolHandler* CreateProtocolHandler(const DictionaryValue* value);

  // Interpolates the given URL into the URL template of this handler.
  GURL TranslateUrl(const GURL& url);

  // Encodes this protocol handler as a Value. The caller is responsible for
  // deleting the returned value.
  Value* Encode();

  std::string protocol() const { return protocol_; }
  GURL url() const { return url_;}
  string16 title() const { return title_; }

  bool operator==(const ProtocolHandler &other) const;

 private:
  ProtocolHandler(const std::string& protocol,
                  const GURL& url,
                  const string16& title);
  const std::string protocol_;
  const GURL url_;
  const string16 title_;
};

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_H_

