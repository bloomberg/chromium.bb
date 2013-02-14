// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/custom_handlers/protocol_handler.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"


ProtocolHandler::ProtocolHandler(const std::string& protocol,
                                 const GURL& url,
                                 const string16& title)
    : protocol_(protocol),
      url_(url),
      title_(title) {
}

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    const string16& title) {
  std::string lower_protocol = StringToLowerASCII(protocol);
  return ProtocolHandler(lower_protocol, url, title);
}

ProtocolHandler::ProtocolHandler() {
}

bool ProtocolHandler::IsValidDict(const base::DictionaryValue* value) {
  return value->HasKey("protocol") && value->HasKey("url") &&
    value->HasKey("title");
}

bool ProtocolHandler::IsSameOrigin(
    const ProtocolHandler& handler) const {
  return handler.url().GetOrigin() == url_.GetOrigin();
}

const ProtocolHandler& ProtocolHandler::EmptyProtocolHandler() {
  static const ProtocolHandler* const kEmpty = new ProtocolHandler();
  return *kEmpty;
}

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const base::DictionaryValue* value) {
  if (!IsValidDict(value)) {
    return EmptyProtocolHandler();
  }
  std::string protocol, url;
  string16 title;
  value->GetString("protocol", &protocol);
  value->GetString("url", &url);
  value->GetString("title", &title);
  return ProtocolHandler::CreateProtocolHandler(protocol, GURL(url), title);
}

GURL ProtocolHandler::TranslateUrl(const GURL& url) const {
  std::string translatedUrlSpec(url_.spec());
  ReplaceSubstringsAfterOffset(&translatedUrlSpec, 0, "%s",
      net::EscapeQueryParamValue(url.spec(), true));
  return GURL(translatedUrlSpec);
}

base::DictionaryValue* ProtocolHandler::Encode() const {
  base::DictionaryValue* d = new base::DictionaryValue();
  d->Set("protocol", new base::StringValue(protocol_));
  d->Set("url", new base::StringValue(url_.spec()));
  d->Set("title", new base::StringValue(title_));
  return d;
}

#if !defined(NDEBUG)
std::string ProtocolHandler::ToString() const {
  return "{ protocol=" + protocol_ +
         ", url=" + url_.spec() +
         ", title=" + UTF16ToASCII(title_) +
         " }";
}
#endif

bool ProtocolHandler::operator==(const ProtocolHandler& other) const {
  return protocol_ == other.protocol_ &&
    url_ == other.url_ &&
    title_ == other.title_;
}

bool ProtocolHandler::IsEquivalent(const ProtocolHandler& other) const {
  return protocol_ == other.protocol_ && url_ == other.url_;
}

bool ProtocolHandler::operator<(const ProtocolHandler& other) const {
  return title_ < other.title_;
}
