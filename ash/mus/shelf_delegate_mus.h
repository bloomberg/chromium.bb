// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHELF_DELEGATE_MUS_H_
#define ASH_MUS_SHELF_DELEGATE_MUS_H_

#include <string>

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/public/cpp/shelf_types.h"

namespace ash {

// Manages communication between the mash shelf and the browser.
class ShelfDelegateMus : public ShelfDelegate {
 public:
  ShelfDelegateMus();
  ~ShelfDelegateMus() override;

 private:
  // ShelfDelegate:
  ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  ShelfID GetShelfIDForAppIDAndLaunchID(const std::string& app_id,
                                        const std::string& launch_id) override;
  bool HasShelfIDToAppIDMapping(ShelfID id) const override;
  const std::string& GetAppIDForShelfID(ShelfID id) override;
  void PinAppWithID(const std::string& app_id) override;
  bool IsAppPinned(const std::string& app_id) override;
  void UnpinAppWithID(const std::string& app_id) override;

  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_SHELF_DELEGATE_MUS_H_
