// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_URL_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_URL_PROVIDER_H_

#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;
class ClipboardRecentContent;

// Autocomplete provider offering content based on the clipboard's content.
class ClipboardURLProvider : public AutocompleteProvider {
 public:
  ClipboardURLProvider(AutocompleteProviderClient* client,
                       ClipboardRecentContent* clipboard_content);

  // AutocompleteProvider implementation.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~ClipboardURLProvider() override;

  AutocompleteProviderClient* client_;
  ClipboardRecentContent* clipboard_content_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardURLProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_URL_PROVIDER_H_
