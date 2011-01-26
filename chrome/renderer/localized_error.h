// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_LOCALIZED_ERROR_H_
#define CHROME_RENDERER_LOCALIZED_ERROR_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class DictionaryValue;
class Extension;
class GURL;

namespace WebKit {
struct WebURLError;
}

class LocalizedError {
 public:
  // Fills |error_strings| with values to be used to build an error page used
  // on HTTP errors, like 404 or connection reset.
  static void GetStrings(const WebKit::WebURLError& error,
                         DictionaryValue* strings);

  // Returns true if an error page exists for the specified parameters.
  static bool HasStrings(const std::string& error_domain, int error_code);

  // Fills |error_strings| with values to be used to build an error page which
  // warns against reposting form data. This is special cased because the form
  // repost "error page" has no real error associated with it, and doesn't have
  // enough strings localized to meaningfully fill the net error template.
  static void GetFormRepostStrings(const GURL& display_url,
                                   DictionaryValue* error_strings);

  // Fills |error_strings| with values to be used to build an error page used
  // on HTTP errors, like 404 or connection reset, but using information from
  // the associated |app| in order to make the error page look like it's more
  // part of the app.
  static void GetAppErrorStrings(const WebKit::WebURLError& error,
                                 const GURL& display_url,
                                 const Extension* app,
                                 DictionaryValue* error_strings);

  static const char kHttpErrorDomain[];

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalizedError);
};

#endif  // CHROME_RENDERER_LOCALIZED_ERROR_H_
