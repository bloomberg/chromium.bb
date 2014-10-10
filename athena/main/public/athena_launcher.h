// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_PUBLIC_ATHENA_LAUNCHER_H_
#define ATHENA_MAIN_PUBLIC_ATHENA_LAUNCHER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class TaskRunner;
}

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
}

namespace athena {
class ActivityFactory;
class AppModelBuilder;
class SearchControllerFactory;

// Starts down the athena shell environment.
void StartAthenaEnv(scoped_refptr<base::TaskRunner> file_runner);

void StartAthenaSessionWithContext(content::BrowserContext* context);

void CreateVirtualKeyboardWithContext(content::BrowserContext* context);

// Starts the athena session.
void StartAthenaSession(ActivityFactory* activity_factory,
                        scoped_ptr<AppModelBuilder> app_model_builder,
                        scoped_ptr<SearchControllerFactory> search_factory);

void ShutdownAthena();

}  // namespace athena

#endif  // ATHENA_MAIN_PUBLIC_ATHENA_LAUNCHER_H_
