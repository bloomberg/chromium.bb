// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_PUBLIC_APPS_SEARCH_CONTROLLER_FACTORY_H_
#define ATHENA_EXTENSIONS_PUBLIC_APPS_SEARCH_CONTROLLER_FACTORY_H_

#include "athena/athena_export.h"

namespace content {
class BrowserContext;
}

namespace athena {
class SearchControllerFactory;

ATHENA_EXPORT scoped_ptr<SearchControllerFactory> CreateSearchControllerFactory(
    content::BrowserContext* context);

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_PUBLIC_APPS_SEARCH_CONTROLLER_FACTORY_H_
