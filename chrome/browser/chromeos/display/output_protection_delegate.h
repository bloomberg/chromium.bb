// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/manager/chromeos/display_configurator.h"

namespace chromeos {

// A class to query output protection status and/or enable output protection.
// All methods except constructor should be invoked in UI thread.
class OutputProtectionDelegate : public aura::WindowObserver {
 public:
  typedef base::Callback<void(bool /* success */,
                              uint32_t /* link_mask */,
                              uint32_t /* protection_mask*/)>
      QueryStatusCallback;
  typedef base::Callback<void(bool /* success */)> EnableProtectionCallback;

  OutputProtectionDelegate(int render_process_id, int render_frame_id);
  ~OutputProtectionDelegate() override;

  // aura::WindowObserver overrides.
  void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  void QueryStatus(const QueryStatusCallback& callback);
  void EnableProtection(uint32_t desired_method_mask,
                        const EnableProtectionCallback& callback);

 private:
  ui::DisplayConfigurator::ContentProtectionClientId GetClientId();

  void QueryStatusComplete(
      const QueryStatusCallback& callback,
      const ui::DisplayConfigurator::QueryProtectionResponse& response);
  void EnableProtectionComplete(const EnableProtectionCallback& callback,
                                bool success);

  // Used to lookup the WebContents associated with the render frame.
  int render_process_id_;
  int render_frame_id_;

  // Native window being observed.
  aura::Window* window_;

  ui::DisplayConfigurator::ContentProtectionClientId client_id_;

  // The display id which the renderer currently uses.
  int64_t display_id_;

  // The last desired method mask. Will enable this mask on new display if
  // renderer changes display.
  uint32_t desired_method_mask_;

  base::WeakPtrFactory<OutputProtectionDelegate> weak_ptr_factory_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_
