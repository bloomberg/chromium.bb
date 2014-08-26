// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extensions_delegate.h"

#include "base/logging.h"

namespace athena {
namespace {

ExtensionsDelegate* instance = NULL;

}  // namespace

ExtensionsDelegate::ExtensionsDelegate() {
  DCHECK(!instance);
  instance = this;
}

ExtensionsDelegate::~ExtensionsDelegate() {
  DCHECK(instance);
  instance = NULL;
}

// static
ExtensionsDelegate* ExtensionsDelegate::Get(content::BrowserContext* context) {
  DCHECK(instance);
  DCHECK_EQ(context, instance->GetBrowserContext());
  return instance;
}

// static
void ExtensionsDelegate::Shutdown() {
  DCHECK(instance);
  delete instance;
}

}  // namespace athena
