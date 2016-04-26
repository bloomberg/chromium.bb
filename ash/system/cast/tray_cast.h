// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_TRAY_CAST_H_
#define ASH_SYSTEM_CAST_TRAY_CAST_H_

#include "ash/cast_config_delegate.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace ash {
namespace tray {
class CastTrayView;
class CastSelectDefaultView;
class CastDetailedView;
class CastDuplexView;
}  // namespace tray

class ASH_EXPORT TrayCast : public SystemTrayItem,
                            public ShellObserver,
                            public CastConfigDelegate::Observer {
 public:
  explicit TrayCast(SystemTray* system_tray);
  ~TrayCast() override;

 private:
  // Helper/utility methods for testing.
  friend class TrayCastTestAPI;
  void StartCastForTest(const std::string& receiver_id);
  void StopCastForTest();
  // Returns the id of the item we are currently displaying in the cast view.
  // This assumes that the cast view is active.
  const std::string& GetDisplayedCastId();
  const views::View* GetDefaultView() const;
  enum ChildViewId { TRAY_VIEW = 1, SELECT_VIEW, CAST_VIEW };

  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterShelfAlignmentChange(wm::ShelfAlignment alignment) override;

  // Overridden from ShellObserver.
  void OnCastingSessionStartedOrStopped(bool started) override;

  // Overridden from CastConfigDelegate::Observer.
  void OnDevicesUpdated(
      const CastConfigDelegate::ReceiversAndActivities& devices) override;

  // Returns true if the cast extension was detected.
  bool HasCastExtension();

  // This makes sure that the current view displayed in the tray is the correct
  // one, depending on if we are currently casting. If we're casting, then a
  // view with a stop button is displayed; otherwise, a view that links to a
  // detail view is displayed instead that allows the user to easily begin a
  // casting session.
  void UpdatePrimaryView();

  CastConfigDelegate::ReceiversAndActivities receivers_and_activities_;
  bool is_casting_ = false;

  bool added_observer_ = false;

  // Not owned.
  tray::CastTrayView* tray_ = nullptr;
  tray::CastDuplexView* default_ = nullptr;
  tray::CastDetailedView* detailed_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TrayCast);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_TRAY_CAST_H_
