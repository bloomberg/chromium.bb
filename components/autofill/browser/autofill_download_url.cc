// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/autofill_download_url.h"

#include <string>

#include "base/command_line.h"
#include "components/autofill/common/autofill_switches.h"
#include "googleurl/src/gurl.h"

namespace autofill {
namespace {

const char kDefaultAutofillServiceUrl[] =
    "https://clients1.google.com/tbproxy/af/";

#if defined(GOOGLE_CHROME_BUILD)
const char kClientName[] = "Google Chrome";
#else
const char kClientName[] = "Chromium";
#endif  // defined(GOOGLE_CHROME_BUILD)

std::string GetBaseAutofillUrl() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string baseAutofillServiceUrl = command_line.GetSwitchValueASCII(
      switches::kAutofillServiceUrl);
  if (baseAutofillServiceUrl.empty())
    return kDefaultAutofillServiceUrl;

  return baseAutofillServiceUrl;
}

}  // namespace

GURL GetAutofillQueryUrl() {
  std::string baseAutofillServiceUrl = GetBaseAutofillUrl();
  return GURL(baseAutofillServiceUrl + "query?client=" + kClientName);
}

GURL GetAutofillUploadUrl() {
  std::string baseAutofillServiceUrl = GetBaseAutofillUrl();
  return GURL(baseAutofillServiceUrl + "upload?client=" + kClientName);
}

}  // namespace autofill
