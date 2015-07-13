// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GCM_DESKTOP_UTIL_H_
#define CHROME_COMMON_GCM_DESKTOP_UTIL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class PrefService;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace gcm {

class GCMDriver;
class GCMClientFactory;

scoped_ptr<GCMDriver> CreateGCMDriverDesktopWithTaskRunners(
    scoped_ptr<GCMClientFactory> gcm_client_factory,
    PrefService* prefs,
    const base::FilePath& store_path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

}  // namespace gcm

#endif  // CHROME_COMMON_GCM_DESKTOP_UTIL_H_
