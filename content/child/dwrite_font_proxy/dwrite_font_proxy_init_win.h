// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_INIT_WIN_H_
#define CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_INIT_WIN_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"

namespace content {

// Initializes the dwrite font proxy.
CONTENT_EXPORT void InitializeDWriteFontProxy();

// Uninitialize the dwrite font proxy. This is safe to call even if the proxy
// has not been initialized. After this, calls to load fonts may fail.
CONTENT_EXPORT void UninitializeDWriteFontProxy();

// Configures the dwrite font proxy to use the specified sender. This can be
// useful in tests which use a fake render thread which is unable to process
// font IPC messages. This should only be called when running as a test.
CONTENT_EXPORT void SetDWriteFontProxySenderForTesting(IPC::Sender* sender);

// Allows ChildThreadImpl to register a thread saef sender to DWriteFontProxy
// so we don't depend on being on the main thread to use DWriteFontProxy.
CONTENT_EXPORT void UpdateDWriteFontProxySender(IPC::Sender*);

}  // namespace content

#endif  // CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_INIT_WIN_H_
