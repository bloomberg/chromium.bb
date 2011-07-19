// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/cookie_commands.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/test/webdriver/cookie.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/commands/response.h"

namespace {
// The first build number that the new automation JSON interface for setting,
// deleting, and getting detailed cookies is availble for.
const int kNewInterfaceBuildNo = 716;
}

namespace webdriver {

CookieCommand::CookieCommand(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

CookieCommand::~CookieCommand() {}

bool CookieCommand::Init(Response* const response) {
  if (!WebDriverCommand::Init(response))
    return false;

  Error* error = session_->CompareBrowserVersion(
      kNewInterfaceBuildNo, 0, &uses_new_interface_);
  if (error) {
    response->SetError(error);
    return false;
  }
  error = session_->GetURL(&current_url_);
  if (error) {
    response->SetError(error);
    return false;
  }
  return true;
}

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
  if (uses_new_interface_) {
    ListValue* cookies;
    Error* error = session_->GetCookies(current_url_.possibly_invalid_spec(),
                                        &cookies);
    if (error) {
      response->SetError(error);
      return;
    }
    response->SetStatus(kSuccess);
    response->SetValue(cookies);
  } else {
    std::string cookies;
    std::vector<std::string> tokens;
    if (!session_->GetCookiesDeprecated(current_url_, &cookies)) {
      response->SetError(new Error(kUnknownError, "Failed to fetch cookies"));
      return;
    }
    // Note that ';' separates cookies while ':' separates name/value pairs.
    Tokenize(cookies, ";", &tokens);
    scoped_ptr<ListValue> cookie_list(new ListValue());
    for (std::vector<std::string>::iterator i = tokens.begin();
         i != tokens.end(); ++i) {
      Cookie cookie(*i);
      if (cookie.valid()) {
        cookie_list->Append(cookie.ToDictionary());
      } else {
        response->SetError(new Error(kUnknownError, "Failed to parse cookies"));
        return;
      }
    }
    response->SetStatus(kSuccess);
    response->SetValue(cookie_list.release());
  }
}

void CookieCommand::ExecutePost(Response* const response) {
  std::string cookie_string;
  DictionaryValue* cookie_dict;

  // Check to see if the cookie was passed in as a JSON onject.
  if (!GetDictionaryParameter("cookie", &cookie_dict)) {
    response->SetError(new Error(kBadRequest, "Cookie key not found"));
    return;
  }

  if (uses_new_interface_) {
    std::string domain;
    if (cookie_dict->GetString("domain", &domain)) {
      std::vector<std::string> split_domain;
      base::SplitString(domain, ':', &split_domain);
      if (split_domain.size() > 2) {
        response->SetError(new Error(
            kInvalidCookieDomain, "Cookie domain has too many colons"));
        return;
      } else if (split_domain.size() == 2) {
        // Remove the port number.
        cookie_dict->SetString("domain", split_domain[0]);
      }
    }
    Error* error = session_->SetCookie(
        current_url_.possibly_invalid_spec(), cookie_dict);
    if (error) {
      response->SetError(error);
      return;
    }
  } else {
    Cookie cookie(*cookie_dict);

    // Make sure the cookie is formated preoperly.
    if (!cookie.valid()) {
      response->SetError(new Error(kBadRequest, "Invalid cookie"));
       return;
    }

    if (!session_->SetCookieDeprecated(current_url_, cookie.ToString())) {
      response->SetError(new Error(kUnableToSetCookie, "Failed to set cookie"));
      return;
    }
    response->SetValue(new StringValue(cookie.ToString()));
  }
  response->SetStatus(kSuccess);
}

void CookieCommand::ExecuteDelete(Response* const response) {
  if (uses_new_interface_) {
    ListValue* unscoped_cookies;
    Error* error = session_->GetCookies(
        current_url_.possibly_invalid_spec(), &unscoped_cookies);
    if (error) {
      response->SetError(error);
      return;
    }
    scoped_ptr<ListValue> cookies(unscoped_cookies);
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
      Error* error = session_->DeleteCookie(
          current_url_.possibly_invalid_spec(), name);
      if (error) {
        response->SetError(error);
        return;
      }
    }
  } else {
    std::string cookies;
    std::vector<std::string> tokens;

    if (!session_->GetCookiesDeprecated(current_url_, &cookies)) {
      response->SetError(new Error(
          kUnableToSetCookie, "Failed to fetch cookies"));
      return;
    }

    Tokenize(cookies, ":", &tokens);
    for (std::vector<std::string>::iterator i = tokens.begin();
         i != tokens.end(); ++i) {
      Cookie cookie(*i);
      if (cookie.valid()) {
        if (!session_->DeleteCookie(current_url_.possibly_invalid_spec(),
                                    cookie.name())) {
          response->SetError(new Error(
              kUnknownError, "Could not delete all cookies"));
          return;
        }
      } else {
        response->SetError(new Error(kUnknownError, "Failed to parse cookie"));
        return;
      }
    }
  }

  response->SetStatus(kSuccess);
}

NamedCookieCommand::NamedCookieCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

NamedCookieCommand::~NamedCookieCommand() {}

bool NamedCookieCommand::Init(Response* const response) {
  if (!WebDriverCommand::Init(response))
    return false;

  Error* error = session_->CompareBrowserVersion(
      kNewInterfaceBuildNo, 0, &uses_new_interface_);
  if (error)  {
    response->SetError(error);
    return false;
  }

  error = session_->GetURL(&current_url_);
  if (error) {
    response->SetError(error);
    return false;
  }

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
  if (uses_new_interface_) {
    Error* error = session_->DeleteCookie(
        current_url_.possibly_invalid_spec(), cookie_name_);
    if (error) {
      response->SetError(error);
      return;
    }
  } else {
    if (!session_->DeleteCookieDeprecated(current_url_, cookie_name_)) {
      response->SetError(new Error(kUnknownError, "Failed to delete cookie"));
      return;
    }
  }
  response->SetStatus(kSuccess);
}

}  // namespace webdriver
