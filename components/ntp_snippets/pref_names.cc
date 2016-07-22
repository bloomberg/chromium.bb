// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/pref_names.h"

namespace ntp_snippets {
namespace prefs {

const char kEnableSnippets[] = "ntp_snippets.enable";

const char kSnippetHosts[] = "ntp_snippets.hosts";

const char kSnippetFetcherQuotaDay[] =
    "ntp.request_throttler.suggestion_fetcher.day";
const char kSnippetFetcherQuotaCount[] =
    "ntp.request_throttler.suggestion_fetcher.count";

}  // namespace prefs
}  // namespace ntp_snippets
