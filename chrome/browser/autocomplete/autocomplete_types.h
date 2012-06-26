// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_TYPES_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_TYPES_H_
#pragma once

#include <vector>

#include "chrome/common/metrics/proto/omnibox_event.pb.h"

class AutocompleteProvider;
struct AutocompleteMatch;

typedef std::vector<AutocompleteMatch> ACMatches;
typedef std::vector<AutocompleteProvider*> ACProviders;
typedef std::vector<metrics::OmniboxEventProto_ProviderInfo> ProvidersInfo;

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_TYPES_H_
