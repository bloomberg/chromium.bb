// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/cookie_commands.h"

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/test/webdriver/cookie.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/commands/response.h"

namespace webdriver {

CookieCommand::CookieCommand(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

CookieCommand::~CookieCommand() {}

bool CookieCommand::Init(Response* const response) {
  if (WebDriverCommand::Init(response)) {
    if (session_->GetURL(&current_url_)) {
      return true;
    }
    SET_WEBDRIVER_ERROR(response, "Failed to query current page URL",
                        kInternalServerError);
    return false;
  }

  return false;
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
  // TODO(JMikhail): Add GetJSONCookies to automation proxy since
  // GetCookies does not return the necessary information
  std::string cookies;
  std::vector<std::string> tokens;

  if (!session_->GetCookies(current_url_, &cookies)) {
    SET_WEBDRIVER_ERROR(response, "Failed to fetch cookies",
                        kUnknownError);
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
      LOG(ERROR) << "Failed to parse cookie: " << *i;
      SET_WEBDRIVER_ERROR(response, "Could not get all cookies",
                          kInternalServerError);
      return;
    }
  }

  response->SetStatus(kSuccess);
  response->SetValue(cookie_list.release());
}

void CookieCommand::ExecutePost(Response* const response) {
  std::string cookie_string;
  DictionaryValue* cookie_dict;

  // Check to see if the cookie was passed in as a JSON onject.
  if (!GetDictionaryParameter("cookie", &cookie_dict)) {
    // No valid cookie sent.
    SET_WEBDRIVER_ERROR(response, "Cookie key not found",
                        kBadRequest);
    return;
  }
  Cookie cookie(*cookie_dict);

  // Make sure the cookie is formated preoperly.
  if (!cookie.valid()) {
    SET_WEBDRIVER_ERROR(response, "Invalid cookie",
                        kBadRequest);
     return;
  }

  if (!session_->SetCookie(current_url_, cookie.ToString())) {
    SET_WEBDRIVER_ERROR(response, "Failed to set cookie",
                        kUnableToSetCookie);
    return;
  }

  response->SetStatus(kSuccess);
  response->SetValue(new StringValue(cookie.ToString()));
}

void CookieCommand::ExecuteDelete(Response* const response) {
  std::string cookies;
  std::vector<std::string> tokens;

  if (!session_->GetCookies(current_url_, &cookies)) {
    SET_WEBDRIVER_ERROR(response, "Failed to fetch cookies",
                        kUnableToSetCookie);
    return;
  }

  Tokenize(cookies, ":", &tokens);
  for (std::vector<std::string>::iterator i = tokens.begin();
       i != tokens.end(); ++i) {
    Cookie cookie(*i);
    if (cookie.valid()) {
      if (!session_->DeleteCookie(current_url_, cookie.name())) {
        VLOG(1) << "Could not delete cookie: " << cookie.name() << "\n"
                << "Contents of cookie: " << cookie.ToString();
        SET_WEBDRIVER_ERROR(response, "Could not delete all cookies",
                            kInternalServerError);
        return;
      }
    } else {
      LOG(ERROR) << "Failed to parse cookie: " << *i;
      SET_WEBDRIVER_ERROR(response, "Could not delete all cookies",
                          kInternalServerError);
      return;
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
  if (WebDriverCommand::Init(response)) {
    if (!session_->GetURL(&current_url_)) {
      SET_WEBDRIVER_ERROR(response, "Failed to query current page URL",
                          kInternalServerError);
      return false;
    }

    // There should be at least 5 segments to match
    // /session/:sessionId/cookie/:name
    cookie_name_ = GetPathVariable(4);
    if (cookie_name_ == "") {
      SET_WEBDRIVER_ERROR(response, "No cookie name specified",
                          kBadRequest);
      return false;
    }

    return true;
  }

  return false;
}

bool NamedCookieCommand::DoesDelete() {
  return true;
}

bool NamedCookieCommand::DoesGet() {
  return true;
}

void NamedCookieCommand::ExecuteGet(Response* const response) {
  std::string cookie;

  if (!session_->GetCookieByName(current_url_, cookie_name_, &cookie)) {
    SET_WEBDRIVER_ERROR(response, "Failed to fetch cookie",
                        kUnknownError);
    return;
  }

  response->SetStatus(kSuccess);
  response->SetValue(new StringValue(cookie));
}

void NamedCookieCommand::ExecuteDelete(Response* const response) {
  if (!session_->DeleteCookie(current_url_, cookie_name_)) {
    SET_WEBDRIVER_ERROR(response, "Failed to delete cookie",
                        kUnknownError);
    return;
  }

  response->SetStatus(kSuccess);
}

}  // namespace webdriver

