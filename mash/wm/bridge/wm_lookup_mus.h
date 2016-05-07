// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_BRIDGE_WM_LOOKUP_MUS_H_
#define MASH_WM_BRIDGE_WM_LOOKUP_MUS_H_

#include <stdint.h>

#include "ash/wm/common/wm_lookup.h"
#include "base/macros.h"

namespace mash {
namespace wm {

// WmLookup implementation for mus.
class WmLookupMus : public ash::wm::WmLookup {
 public:
  WmLookupMus();
  ~WmLookupMus() override;

  // ash::wm::WmLookup:
  ash::wm::WmRootWindowController* GetRootWindowControllerWithDisplayId(
      int64_t id) override;
  ash::wm::WmWindow* GetWindowForWidget(views::Widget* widget) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmLookupMus);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_BRIDGE_WM_LOOKUP_MUS_H_
