// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_DISCOVERY_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_DISCOVERY_H_
#pragma once

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/ntp/suggestions_source.h"

class SuggestionsCombiner;
class Profile;

namespace base {
class DictionaryValue;
}

// A source linked to a single extension using the discovery API to suggest
// websites the user might find interesting.
class SuggestionsSourceDiscovery : public SuggestionsSource {
 public:
  explicit SuggestionsSourceDiscovery(const std::string& extension_id);
  virtual ~SuggestionsSourceDiscovery();

 protected:
  // SuggestionsSource overrides:
  virtual void SetDebug(bool enable) OVERRIDE;
  virtual int GetWeight() OVERRIDE;
  virtual int GetItemCount() OVERRIDE;
  virtual base::DictionaryValue* PopItem() OVERRIDE;
  virtual void FetchItems(Profile* profile) OVERRIDE;
  virtual void SetCombiner(SuggestionsCombiner* combiner) OVERRIDE;

 private:
  // Our combiner.
  SuggestionsCombiner* combiner_;

  // Keep the results fetched from the discovery service here.
  std::deque<base::DictionaryValue*> items_;

  // The extension associated with this source.
  std::string extension_id_;

  // Whether the source should provide additional debug information or not.
  bool debug_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSourceDiscovery);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_SUGGESTIONS_SOURCE_DISCOVERY_H_
