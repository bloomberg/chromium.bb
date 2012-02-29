// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// static
void RulesRegistryDeleteTraits::Destruct(const RulesRegistry* rules_registry) {
  content::BrowserThread::DeleteSoon(rules_registry->GetOwnerThread(),
      FROM_HERE, rules_registry);
}

}  // extensions
