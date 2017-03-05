// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ACCESSIBILITY_DELEGATE_MUS_H_
#define ASH_MUS_ACCESSIBILITY_DELEGATE_MUS_H_

#include "ash/common/default_accessibility_delegate.h"
#include "base/macros.h"
#include "services/ui/public/interfaces/accessibility_manager.mojom.h"

namespace service_manager {
class Connector;
}

namespace ash {

class AccessibilityDelegateMus : public DefaultAccessibilityDelegate {
 public:
  explicit AccessibilityDelegateMus(service_manager::Connector* connector);
  ~AccessibilityDelegateMus() override;

 private:
  ui::mojom::AccessibilityManager* GetAccessibilityManager();

  // DefaultAccessibilityDelegate:
  void ToggleHighContrast() override;

  ui::mojom::AccessibilityManagerPtr accessibility_manager_ptr_;
  service_manager::Connector* connector_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_ACCESSIBILITY_DELEGATE_MUS_H_
