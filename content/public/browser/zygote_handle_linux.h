// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ZYGOTE_HANDLE_LINUX_H_
#define CONTENT_PUBLIC_BROWSER_ZYGOTE_HANDLE_LINUX_H_

#include "base/callback.h"
#include "base/command_line.h"
#include "content/common/content_export.h"
#include "content/public/common/zygote_handle.h"

namespace content {

// Allocates and initializes the global generic zygote process, and returns the
// ZygoteHandle used to communicate with it. |add_switches_callback| allows
// adding additional command line switches once the zygote process command line
// has been composed by this function.
CONTENT_EXPORT ZygoteHandle CreateGenericZygote(
    base::OnceCallback<void(base::CommandLine*)> add_switches_callback);

// Returns a handle to a global generic zygote object. This function allows the
// browser to launch and use a single zygote process until the performance
// issues around launching multiple zygotes are resolved.
// http://crbug.com/569191
CONTENT_EXPORT ZygoteHandle GetGenericZygote();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ZYGOTE_HANDLE_LINUX_H_
