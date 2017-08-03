// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/mock_autocomplete_provider_client.h"

#include "base/memory/ptr_util.h"

MockAutocompleteProviderClient::MockAutocompleteProviderClient() {
  contextual_suggestions_service_ =
      base::MakeUnique<ContextualSuggestionsService>(
          /*signin_manager=*/nullptr, /*token_service=*/nullptr,
          GetRequestContext());
}

MockAutocompleteProviderClient::~MockAutocompleteProviderClient() {
}
