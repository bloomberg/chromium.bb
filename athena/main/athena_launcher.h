// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_ATHENA_LAUNCHER_H_
#define ATHENA_MAIN_ATHENA_LAUNCHER_H_

namespace aura {
class Window;
}

namespace athena {
class ActivityFactory;
class AppModelBuilder;

// Starts/shuts down the athena shell environment.
void StartAthena(aura::Window* root_window,
                 ActivityFactory* activity_factory,
                 AppModelBuilder* app_model_builder);
void ShutdownAthena();

}  // namespace athena

#endif  // ATHENA_MAIN_ATHENA_LAUNCHER_H_
