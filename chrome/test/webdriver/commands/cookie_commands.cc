// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/cookie_commands.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_session.h"

namespace webdriver {

CookieCommand::CookieCommand(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

CookieCommand::~CookieCommand() {}

bool CookieCommand::DoesDelete() {
  return true;
}

bool CookieCommand::DoesGet() {
  return true;
}

bool CookieCommand::DoesPost() {
  return true;
}

void CookieCommand::ExecuteGet(Response* const response) {
  std::string url;
  Error* error = session_->GetURL(&url);
  ListValue* cookies;
  if (!error)
    error = session_->GetCookies(url, &cookies);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(cookies);
}

void CookieCommand::ExecutePost(Response* const response) {
  const DictionaryValue* cookie_dict;
  if (!GetDictionaryParameter("cookie", &cookie_dict)) {
    response->SetError(new Error(
        kBadRequest, "Missing or invalid |cookie| parameter"));
    return;
  }
  scoped_ptr<DictionaryValue> cookie_dict_copy(cookie_dict->DeepCopy());

  std::string domain;
  if (cookie_dict_copy->GetString("domain", &domain)) {
    std::vector<std::string> split_domain;
    base::SplitString(domain, ':', &split_domain);
    if (split_domain.size() > 2) {
      response->SetError(new Error(
          kInvalidCookieDomain, "Cookie domain has too many colons"));
      return;
    } else if (split_domain.size() == 2) {
      // Remove the port number.
      cookie_dict_copy->SetString("domain", split_domain[0]);
    }
  }

  std::string url;
  Error* error = session_->GetURL(&url);
  if (!error)
    error = session_->SetCookie(url, cookie_dict_copy.get());
  if (error) {
    response->SetError(error);
    return;
  }
}

void CookieCommand::ExecuteDelete(Response* const response) {
  std::string url;
  Error* error = session_->GetURL(&url);
  ListValue* unscoped_cookies = NULL;
  if (!error)
    error = session_->GetCookies(url, &unscoped_cookies);
  scoped_ptr<ListValue> cookies(unscoped_cookies);
  if (error) {
    response->SetError(error);
    return;
  }

  for (size_t i = 0; i < cookies->GetSize(); ++i) {
    DictionaryValue* cookie_dict;
    if (!cookies->GetDictionary(i, &cookie_dict)) {
      response->SetError(new Error(
          kUnknownError, "GetCookies returned non-dict type"));
      return;
    }
    std::string name;
    if (!cookie_dict->GetString("name", &name)) {
      response->SetError(new Error(
          kUnknownError,
          "GetCookies returned cookie with missing or invalid 'name'"));
      return;
    }
    error = session_->DeleteCookie(url, name);
    if (error) {
      response->SetError(error);
      return;
    }
  }
}

NamedCookieCommand::NamedCookieCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

NamedCookieCommand::~NamedCookieCommand() {}

bool NamedCookieCommand::Init(Response* const response) {
  if (!WebDriverCommand::Init(response))
    return false;

  // There should be at least 5 segments to match
  // /session/:sessionId/cookie/:name
  cookie_name_ = GetPathVariable(4);
  if (cookie_name_ == "") {
    response->SetError(new Error(kBadRequest, "No cookie name specified"));
    return false;
  }

  return true;
}

bool NamedCookieCommand::DoesDelete() {
  return true;
}

void NamedCookieCommand::ExecuteDelete(Response* const response) {
  std::string url;
  Error* error = session_->GetURL(&url);
  if (!error)
    error = session_->DeleteCookie(url, cookie_name_);
  if (error) {
    response->SetError(error);
    return;
  }
}

}  // namespace webdriver
