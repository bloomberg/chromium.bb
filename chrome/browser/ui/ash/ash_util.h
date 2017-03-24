// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_UTIL_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"

namespace service_manager {
class Service;
}

namespace ui {
class Accelerator;
class KeyEvent;
}  // namespace ui

namespace ash_util {

enum class Config {
  // Classic mode does not use mus.
  CLASSIC,

  // Aura is backed by mus, but chrome and ash are still in the same process.
  MUS,

  // Aura is backed by mus and chrome and ash are in separate processes. In this
  // mode chrome code can only use ash code in ash/public/cpp.
  MASH,
};

// Creates an in-process Service instance of which can host common ash
// interfaces.
std::unique_ptr<service_manager::Service> CreateEmbeddedAshService(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

// Returns true if Ash should be run at startup.
bool ShouldOpenAshOnStartup();

// Returns true if Chrome is running in the mash shell.
// TODO(sky): convert to GetConfig().
bool IsRunningInMash();

Config GetConfig();

// Returns true if the given |accelerator| has been deprecated and hence can
// be consumed by web contents if needed.
bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator);

// Returns true if ash has an accelerator for |key_event| that is enabled.
bool WillAshProcessAcceleratorForEvent(const ui::KeyEvent& key_event);

}  // namespace ash_util

#endif  // CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
