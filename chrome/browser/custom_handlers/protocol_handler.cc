// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler.h"

#include "base/string_util.h"
#include "net/base/escape.h"

ProtocolHandler::ProtocolHandler(const std::string& protocol,
                                 const GURL& url,
                                 const string16& title)
  : protocol_(protocol),
    url_(url),
    title_(title) {
}

ProtocolHandler* ProtocolHandler::CreateProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    const string16& title) {
  std::string lower_protocol(protocol);
  lower_protocol = StringToLowerASCII(protocol);
  return new ProtocolHandler(lower_protocol, url, title);
}

ProtocolHandler* ProtocolHandler::CreateProtocolHandler(
    const DictionaryValue* value) {
  std::string protocol, url;
  string16 title;
  value->GetString("protocol", &protocol);
  value->GetString("url", &url);
  value->GetString("title", &title);
  return ProtocolHandler::CreateProtocolHandler(protocol, GURL(url), title);
}

GURL ProtocolHandler::TranslateUrl(const GURL& url) {
  std::string translatedUrlSpec(url_.spec());
  ReplaceSubstringsAfterOffset(&translatedUrlSpec, 0, "%s",
      EscapeQueryParamValue(url.spec(), true));
  return GURL(translatedUrlSpec);
}

Value* ProtocolHandler::Encode() {
  DictionaryValue* d = new DictionaryValue();
  d->Set("protocol", Value::CreateStringValue(protocol_));
  d->Set("url", Value::CreateStringValue(url_.spec()));
  d->Set("title", Value::CreateStringValue(title_));
  return d;
}

bool ProtocolHandler::operator==(const ProtocolHandler &other) const {
  return protocol_ == other.protocol_ &&
    url_ == other.url_ &&
    title_ == other.title_;
}

