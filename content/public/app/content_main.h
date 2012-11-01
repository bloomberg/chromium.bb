// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_H_

#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {

class ContentMainDelegate;

// ContentMain should be called from the embedder's main() function to do the
// initial setup for every process. The embedder has a chance to customize
// startup using the ContentMainDelegate interface. The embedder can also pass
// in NULL for |delegate| if they don't want to override default startup.
#if defined(OS_WIN)

// |sandbox_info| should be initialized using InitializeSandboxInfo from
// content_main_win.h
CONTENT_EXPORT int ContentMain(HINSTANCE instance,
                               sandbox::SandboxInterfaceInfo* sandbox_info,
                               ContentMainDelegate* delegate);
#elif defined(OS_ANDROID)
// In the Android, the content main starts from ContentMain.java, This function
// provides a way to set the |delegate| as ContentMainDelegate for
// ContentMainRunner.
// This should only be called once before ContentMainRunner actually running.
// The ownership of |delegate| is transferred.
CONTENT_EXPORT void SetContentMainDelegate(ContentMainDelegate* delegate);
#else
CONTENT_EXPORT int ContentMain(int argc,
                               const char** argv,
                               ContentMainDelegate* delegate);
#endif  // defined(OS_WIN)

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
