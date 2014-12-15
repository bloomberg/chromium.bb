// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/ios/browser_state_helper.h"

#include "base/logging.h"
#include "base/supports_user_data.h"
#include "ios/web/public/browser_state.h"

// iOS code is still using BrowserContextKeyedServiceFactory and until the
// upstreaming is complete (http://crbug.com/419366) there is need to have
// mixed dependency between BCKSF and BSKSF.
//
// The implementation has BrowserStateKeyedServiceFactory supporting a
// BrowserContextDependencyManager as DependencyManager. Thus the context
// parameter passed to the BrowserStateKeyedServiceFactory can either be
// content::BrowserContext if the method is invoked by DependencyManager
// or web::BrowserState if the method is invoked via the type-safe public
// API.
//
// The public API of BrowserStateKeyedServiceFactory is type-safe (all
// public method receive web::BrowserState for context object), so only
// methods that take a base::SupportsUserData need to discriminate
// between the two objects.
//
// If the base::SupportsUserData is a web::BrowserState then the public
// method web::BrowserState::FromSupportsUserData can do the conversion
// safely. If this method fails then context is content::BrowserContext
// and the methods defined below allow the embedder to provides helper
// to find the associated web::BrowserState (there is a 1:1 mapping).

namespace {
BrowserStateFromContextFn g_browser_state_from_context = nullptr;
}  // namespace

void SetBrowserStateFromContextHelper(BrowserStateFromContextFn helper) {
  g_browser_state_from_context = helper;
}

web::BrowserState* BrowserStateFromContext(base::SupportsUserData* context) {
  web::BrowserState* state = nullptr;
  if (context) {
    state = web::BrowserState::FromSupportsUserData(context);
    if (!state && g_browser_state_from_context)
      state = g_browser_state_from_context(context);
    DCHECK(state) << "cannot convert context to web::BrowserState";
  }
  return state;
}
