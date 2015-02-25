// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/signin/scoped_gaia_auth_extension.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/extensions/signin/gaia_auth_extension_loader.h"
#include "content/public/browser/browser_context.h"

ScopedGaiaAuthExtension::ScopedGaiaAuthExtension(
    content::BrowserContext* context)
    : browser_context_(context) {
  extensions::GaiaAuthExtensionLoader* loader =
      extensions::GaiaAuthExtensionLoader::Get(browser_context_);
  if (loader)
    loader->LoadIfNeeded();
}

ScopedGaiaAuthExtension::~ScopedGaiaAuthExtension() {
  extensions::GaiaAuthExtensionLoader* loader =
      extensions::GaiaAuthExtensionLoader::Get(browser_context_);
  if (loader) {
    // Post this instead of calling it directly, to ensure that the
    // RenderFrameHost is not used after being destroyed. This would happen,
    // for example, if we tried to manually navigate to the extension while
    // the <webview> containing the Gaia sign in page (and therefore the
    // extension) was the active tab. See crbug.com/460431.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&extensions::GaiaAuthExtensionLoader::UnloadIfNeeded,
                   base::Unretained(loader)));
  }
}
