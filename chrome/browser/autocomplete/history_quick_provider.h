// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
#pragma once

#include "chrome/browser/autocomplete/autocomplete.h"

// This class is an autocomplete provider (a pseudo-internal component of
// the history system) which quickly (and synchronously) provides matching
// results from recently or frequently visited sites in the profile's
// history.
//
// TODO(mrossetti): Review to see if the following applies since we're not
// using the database during the autocomplete pass.
//
// Note: This object can get leaked on shutdown if there are pending
// requests on the database (which hold a reference to us). Normally, these
// messages get flushed for each thread. We do a round trip from main, to
// history, back to main while holding a reference. If the main thread
// completes before the history thread, the message to delegate back to the
// main thread will not run and the reference will leak. Therefore, don't do
// anything on destruction.
class HistoryQuickProvider : public AutocompleteProvider {
 public:
  HistoryQuickProvider(ACProviderListener* listener, Profile* profile)
      : AutocompleteProvider(listener, profile, "HistoryQuickProvider") {}

  // no destructor (see note above)

  // AutocompleteProvider
  void Start(const AutocompleteInput& input, bool minimal_changes);

 private:
  ~HistoryQuickProvider() {}
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_QUICK_PROVIDER_H_
