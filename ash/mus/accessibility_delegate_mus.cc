// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/accessibility_delegate_mus.h"

#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"

namespace ash {

AccessibilityDelegateMus::AccessibilityDelegateMus(
    service_manager::Connector* connector)
    : connector_(connector) {}

AccessibilityDelegateMus::~AccessibilityDelegateMus() {}

ui::mojom::AccessibilityManager*
AccessibilityDelegateMus::GetAccessibilityManager() {
  if (!accessibility_manager_ptr_.is_bound())
    connector_->BindInterface(ui::mojom::kServiceName,
                              &accessibility_manager_ptr_);
  return accessibility_manager_ptr_.get();
}

void AccessibilityDelegateMus::ToggleHighContrast() {
  DefaultAccessibilityDelegate::ToggleHighContrast();
  GetAccessibilityManager()->SetHighContrastMode(IsHighContrastEnabled());
}

}  // namespace ash
