// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_MANAGER_H_
#define ASH_SYSTEM_TOAST_TOAST_MANAGER_H_

#include <memory>
#include <queue>
#include <string>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/system/toast/toast_overlay.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"

namespace ash {

// Class managing toast requests.
class ASH_EXPORT ToastManager : public ToastOverlay::Delegate {
 public:
  ToastManager();
  ~ToastManager() override;

  // Show a toast. If there are queued toasts, succeeding toasts are queued as
  // well, and are shown one by one.
  void Show(const std::string& text, uint64_t duration_ms);

  // ToastOverlay::Delegate overrides:
  void OnClosed() override;

 private:
  friend class ToastManagerTest;

  void ShowLatest();
  void OnDurationPassed(int toast_id);

  int toast_id_for_testing() const { return toast_id_; }
  ToastOverlay* GetCurrentOverlayForTesting() { return overlay_.get(); }
  void ResetToastIdForTesting() { toast_id_ = 0; }

  int toast_id_ = 0;
  std::queue<std::pair<std::string, uint64_t>> queue_;
  std::unique_ptr<ToastOverlay> overlay_;

  base::WeakPtrFactory<ToastManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ToastManager);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_MANAGER_H_
