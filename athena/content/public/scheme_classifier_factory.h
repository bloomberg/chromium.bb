// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_SCHEME_CLASSIFIER_FACTORY_H_
#define ATHENA_CONTENT_PUBLIC_SCHEME_CLASSIFIER_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "components/omnibox/autocomplete_scheme_classifier.h"

namespace content {
class BrowserContext;
}

namespace athena {

// Create the AutocompleteSchemeClassifier implementation of the current
// environment.
scoped_ptr<AutocompleteSchemeClassifier> CreateSchemeClassifier(
    content::BrowserContext* context);

}  // namespace athena

#endif  // ATHENA_CONTENT_PUBLIC_SCHEME_CLASSIFIER_FACTORY_H_
