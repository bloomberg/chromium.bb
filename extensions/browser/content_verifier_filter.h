// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_FILTER_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_FILTER_H_

#include "base/bind.h"
#include "base/callback.h"

namespace extensions {

class Extension;

// A callback function for deciding if a given extension should have it's
// content verified or not. Returning true means "yes, it should be verified".
//
// This function should be prepared to be called on any thread.
typedef base::Callback<bool(const Extension*)> ContentVerifierFilter;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_FILTER_H_
