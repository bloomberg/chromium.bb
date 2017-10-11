// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_TEST_API_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_TEST_API_H_

#include "ash/public/interfaces/system_tray_test_api.mojom.h"
#include "ash/system/tray/system_tray.h"
#include "base/macros.h"

namespace ash {

// Use by tests to access private state of SystemTray.
class SystemTrayTestApi : public mojom::SystemTrayTestApi {
 public:
  explicit SystemTrayTestApi(SystemTray* tray);
  ~SystemTrayTestApi() override;

  // Creates and binds an instance from a remote request (e.g. from chrome).
  static void BindRequest(mojom::SystemTrayTestApiRequest request);

  TrayAccessibility* tray_accessibility() { return tray_->tray_accessibility_; }
  TrayCapsLock* tray_caps_lock() { return tray_->tray_caps_lock_; }
  TrayCast* tray_cast() { return tray_->tray_cast_; }
  TrayEnterprise* tray_enterprise() { return tray_->tray_enterprise_; }
  TrayNetwork* tray_network() { return tray_->tray_network_; }
  TraySessionLengthLimit* tray_session_length_limit() {
    return tray_->tray_session_length_limit_;
  }
  TraySupervisedUser* tray_supervised_user() {
    return tray_->tray_supervised_user_;
  }
  TrayTracing* tray_tracing() { return tray_->tray_tracing_; }
  TraySystemInfo* tray_system_info() { return tray_->tray_system_info_; }
  TrayTiles* tray_tiles() { return tray_->tray_tiles_; }

  // mojom::SystemTrayTestApi:
  void ShowBubble(ShowBubbleCallback cb) override;
  void ShowDetailedView(mojom::TrayItem item,
                        ShowDetailedViewCallback cb) override;
  void IsBubbleViewVisible(int view_id,
                           IsBubbleViewVisibleCallback cb) override;
  void GetBubbleViewTooltip(int view_id,
                            GetBubbleViewTooltipCallback cb) override;

 private:
  SystemTray* const tray_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_TEST_API_H_
