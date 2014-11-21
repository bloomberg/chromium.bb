// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_HELPER_H_
#define COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_HELPER_H_

namespace base {
class SupportsUserData;
}

namespace web {
class BrowserState;
}

// TODO(sdefresne): remove this file and all usage of the methods once iOS code
// only use BrowserStateKeyedServiceFactory, http://crbug.com/419366

// |BrowserStateFromContextFn| converts from a base::SupportsUserData as passed
// to a (Refcounted)?BrowserStateKeyedServiceFactory to a web::BrowserState.
using BrowserStateFromContextFn =
    web::BrowserState* (*)(base::SupportsUserData*);

// Registers an helper function to convert a |context| to a web::BrowserState
// to allow the embedder to overrides how the BSKSF does the conversion.
void SetBrowserStateFromContextHelper(BrowserStateFromContextFn helper);

// Converts a |context| to a web::BrowserState using the helper registered by
// the embedder if any. Usage is restricted to //components/keyed_service/ios.
web::BrowserState* BrowserStateFromContext(base::SupportsUserData* context);

#endif  // COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_HELPER_H_
