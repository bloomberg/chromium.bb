// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_cookie_handler.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/pipe_reader.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"

namespace chromeos {

void ExternalCookieHandler::GetCookies(const CommandLine& parsed_command_line,
                                       Profile* profile) {
  // If there are Google External SSO cookies, add them to the cookie store.
  if (parsed_command_line.HasSwitch(switches::kCookiePipe)) {
    std::string pipe_name =
        parsed_command_line.GetSwitchValueASCII(switches::kCookiePipe);
    ExternalCookieHandler cookie_handler(new PipeReader(pipe_name));
    cookie_handler.HandleCookies(
        profile->GetRequestContext()->GetCookieStore());
  }
}

// static
const char ExternalCookieHandler::kGoogleAccountsUrl[] =
    "https://www.google.com/a/google.com/acs";

const int kChunkSize = 256;

// Reads up to a newline, or the end of the data, in increments of |chunk|
std::string ExternalCookieHandler::ReadLine(int chunk) {
  std::string cookie_line = reader_->Read(chunk);

  // As long as it's not an empty line...
  if (!cookie_line.empty()) {
    // and there's no newline at the end...
    while ('\n' != cookie_line[cookie_line.length() - 1]) {
      // try to pull more data...
      std::string piece = reader_->Read(chunk);
      if (piece.empty())  // only stop if there's none left.
        break;
      else
        cookie_line.append(piece);  // otherwise, append and keep going.
    }
  }

  return cookie_line;
}

bool ExternalCookieHandler::HandleCookies(net::CookieStore *cookie_store) {
  DCHECK(cookie_store);
  if (NULL != reader_.get()) {
    GURL url(ExternalCookieHandler::kGoogleAccountsUrl);
    net::CookieOptions options;
    options.set_include_httponly();

    // Each line we get is a cookie.  Grab up to a newline, then put
    // it in to the cookie jar.
    std::string cookie_line = ReadLine(kChunkSize);
    while (!cookie_line.empty()) {
      if (!cookie_store->SetCookieWithOptions(url, cookie_line, options))
        return false;
      cookie_line = ReadLine(kChunkSize);
    }
    return true;
  }
  return false;
}

}  // namespace chromeos
