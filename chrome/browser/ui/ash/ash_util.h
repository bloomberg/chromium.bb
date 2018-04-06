// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_UTIL_H_

#include <memory>

#include "ash/public/cpp/config.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "ui/views/widget/widget.h"

namespace service_manager {
class Service;
}

namespace ui {
class Accelerator;
class KeyEvent;
}  // namespace ui

namespace ash_util {

// Creates an in-process Service instance of which can host common ash
// interfaces.
std::unique_ptr<service_manager::Service> CreateEmbeddedAshService(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

// Returns true if Ash should be run at startup.
bool ShouldOpenAshOnStartup();

// Returns true if Chrome is running in the mash shell.
// TODO(sky): convert to chromeos::GetAshConfig() and remove.
bool IsRunningInMash();

// Returns true if the given |accelerator| has been deprecated and hence can
// be consumed by web contents if needed.
bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator);

// Returns true if ash has an accelerator for |key_event| that is enabled.
bool WillAshProcessAcceleratorForEvent(const ui::KeyEvent& key_event);

// Sets up |params| to place the widget in an ash shell window container on
// the primary display. See ash/public/cpp/shell_window_ids.h for |container_id|
// values.
// TODO(jamescook): Extend to take a display_id.
void SetupWidgetInitParamsForContainer(views::Widget::InitParams* params,
                                       int container_id);

}  // namespace ash_util

#endif  // CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
