// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_LOCALIZED_ERROR_H_
#define CHROME_COMMON_LOCALIZED_ERROR_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/error_page/common/offline_page_types.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace error_page {
struct ErrorPageParams;
}

class LocalizedError {
 public:
  // Fills |error_strings| with values to be used to build an error page used
  // on HTTP errors, like 404 or connection reset.
  static void GetStrings(int error_code,
                         const std::string& error_domain,
                         const GURL& failed_url,
                         bool is_post,
                         bool stale_copy_in_cache,
                         bool can_show_network_diagnostics_dialog,
                         error_page::OfflinePageStatus offline_page_status,
                         const std::string& locale,
                         const std::string& accept_languages,
                         scoped_ptr<error_page::ErrorPageParams> params,
                         base::DictionaryValue* strings);

  // Returns a description of the encountered error.
  static base::string16 GetErrorDetails(const std::string& error_domain,
                                        int error_code,
                                        bool is_post);

  // Returns true if an error page exists for the specified parameters.
  static bool HasStrings(const std::string& error_domain, int error_code);

  static const char kHttpErrorDomain[];

 private:

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalizedError);
};

#endif  // CHROME_COMMON_LOCALIZED_ERROR_H_
