// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_URL_PROVIDER_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_URL_PROVIDER_H_

#include "components/omnibox/autocomplete_provider.h"

class ClipboardRecentContent;

// Autocomplete provider offering content based on the clipboard's content.
class ClipboardURLProvider : public AutocompleteProvider {
 public:
  ClipboardURLProvider(ClipboardRecentContent* clipboard_recent_content);
  void Start(const AutocompleteInput& input,
             bool minimal_changes,
             bool called_due_to_focus) override;

 private:
  ~ClipboardURLProvider() override;
  ClipboardRecentContent* clipboard_recent_content_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardURLProvider);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_URL_PROVIDER_H_
