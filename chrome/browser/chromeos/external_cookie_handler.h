// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTERNAL_COOKIE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTERNAL_COOKIE_HANDLER_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/pipe_reader.h"

// Single sign on cookies for Google can be passed in over a
// pipe.  If they've been sent, this reads them and adds them to the
// cookie store as session cookies.

class CommandLine;
class Profile;
namespace net {
class CookieStore;
}

namespace chromeos {

class ExternalCookieHandler {
 public:
  // Takes ownsership of |reader|.
  explicit ExternalCookieHandler(PipeReader *reader) : reader_(reader) {}
  virtual ~ExternalCookieHandler() {}

  // Given a pipe to read cookies from, reads and adds them to |cookie_store|.
  virtual bool HandleCookies(net::CookieStore *cookie_store);

  // The url with which we associate the read-in cookies.
  static const char kGoogleAccountsUrl[];

 private:
  // Reads up to a newline, or the end of the data, in increments of |chunk|.
  std::string ReadLine(int chunk);

  scoped_ptr<PipeReader> reader_;

  DISALLOW_COPY_AND_ASSIGN(ExternalCookieHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTERNAL_COOKIE_HANDLER_H_
