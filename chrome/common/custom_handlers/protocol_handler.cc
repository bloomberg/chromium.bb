// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/custom_handlers/protocol_handler.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

ProtocolHandler::ProtocolHandler(const std::string& protocol,
                                 const GURL& url)
    : protocol_(protocol),
      url_(url) {
}

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const std::string& protocol,
    const GURL& url) {
  std::string lower_protocol = base::ToLowerASCII(protocol);
  return ProtocolHandler(lower_protocol, url);
}

ProtocolHandler::ProtocolHandler() {
}

bool ProtocolHandler::IsValidDict(const base::DictionaryValue* value) {
  // Note that "title" parameter is ignored.
  return value->HasKey("protocol") && value->HasKey("url");
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
  value->GetString("protocol", &protocol);
  value->GetString("url", &url);
  return ProtocolHandler::CreateProtocolHandler(protocol, GURL(url));
}

GURL ProtocolHandler::TranslateUrl(const GURL& url) const {
  std::string translatedUrlSpec(url_.spec());
  base::ReplaceFirstSubstringAfterOffset(
      &translatedUrlSpec, 0, "%s",
      net::EscapeQueryParamValue(url.spec(), true));
  return GURL(translatedUrlSpec);
}

std::unique_ptr<base::DictionaryValue> ProtocolHandler::Encode() const {
  auto d = std::make_unique<base::DictionaryValue>();
  d->SetString("protocol", protocol_);
  d->SetString("url", url_.spec());
  return d;
}

base::string16 ProtocolHandler::GetProtocolDisplayName(
    const std::string& protocol) {
  if (protocol == "mailto")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_MAILTO_NAME);
  if (protocol == "webcal")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_WEBCAL_NAME);
  return base::UTF8ToUTF16(protocol);
}

base::string16 ProtocolHandler::GetProtocolDisplayName() const {
  return GetProtocolDisplayName(protocol_);
}

#if !defined(NDEBUG)
std::string ProtocolHandler::ToString() const {
  return "{ protocol=" + protocol_ +
         ", url=" + url_.spec() +
         " }";
}
#endif

bool ProtocolHandler::operator==(const ProtocolHandler& other) const {
  return protocol_ == other.protocol_ && url_ == other.url_;
}

bool ProtocolHandler::IsEquivalent(const ProtocolHandler& other) const {
  return protocol_ == other.protocol_ && url_ == other.url_;
}

bool ProtocolHandler::operator<(const ProtocolHandler& other) const {
  return url_ < other.url_;
}
