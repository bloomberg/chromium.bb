// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extension_view_host.h"

namespace extensions {

// static
scoped_ptr<ExtensionView> ExtensionViewHost::CreateExtensionView(
    ExtensionViewHost* host,
    Browser* browser) {
  NOTIMPLEMENTED();
  return scoped_ptr<ExtensionView>();
}

}  // namespace extensions
