// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_HANDLE_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_HANDLE_H_

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/zygote_features.h"

#if !BUILDFLAG(USE_ZYGOTE_HANDLE)
#error "Can not use zygote handles without USE_ZYGOTE_HANDLE"
#endif

namespace content {

#if defined(OS_POSIX)
class ZygoteCommunication;
using ZygoteHandle = ZygoteCommunication*;
#else
// Perhaps other ports may USE_ZYGOTE_HANDLE here somdeday.
#error "Can not use zygote handles on this platform"
#endif  // defined(OS_POSIX)

// Allocates and initializes the global generic zygote process, and returns the
// ZygoteHandle used to communicate with it. |launcher| is a callback that
// should actually launch the process, after adding additional command line
// switches to the ones composed by this function. It returns the pid created,
// and provides a control fd for it.
CONTENT_EXPORT ZygoteHandle CreateGenericZygote(
    base::OnceCallback<pid_t(base::CommandLine*, base::ScopedFD*)> launcher);

// Returns a handle to a global generic zygote object. This function allows the
// browser to launch and use a single zygote process until the performance
// issues around launching multiple zygotes are resolved.
// http://crbug.com/569191
CONTENT_EXPORT ZygoteHandle GetGenericZygote();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_HANDLE_H_
