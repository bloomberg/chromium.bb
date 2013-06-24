// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/tray_display.h"

#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/test_system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace internal {

base::string16 GetTooltipText(const base::string16& line1,
                              const base::string16& line2) {
  return line1 + ASCIIToUTF16("\n") + line2;
}

base::string16 GetTooltipText1(const base::string16& line1) {
  return GetTooltipText(line1, base::string16());
}

base::string16 GetTooltipText2(const base::string16& line2) {
  return GetTooltipText(base::string16(), line2);
}

base::string16 GetFirstDisplayName() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  return UTF8ToUTF16(display_manager->GetDisplayNameForId(
      display_manager->first_display_id()));
}

base::string16 GetSecondDisplayName() {
  return UTF8ToUTF16(
      Shell::GetInstance()->display_manager()->GetDisplayNameForId(
          ScreenAsh::GetSecondaryDisplay().id()));
}

base::string16 GetMirroredDisplayName() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  return UTF8ToUTF16(display_manager->GetDisplayNameForId(
      display_manager->mirrored_display().id()));
}

class TrayDisplayTest : public ash::test::AshTestBase {
 public:
  TrayDisplayTest();
  virtual ~TrayDisplayTest();

  virtual void SetUp() OVERRIDE;

 protected:
  SystemTray* tray() { return tray_; }

  void CloseNotification();
  bool IsDisplayVisibleInTray();
  base::string16 GetTrayDisplayText();
  base::string16 GetTrayDisplayTooltipText();
  base::string16 GetDisplayNotificationText();

 private:
  // Weak reference, owned by Shell.
  SystemTray* tray_;

  // Weak reference, owned by |tray_|.
  TrayDisplay* tray_display_;

  DISALLOW_COPY_AND_ASSIGN(TrayDisplayTest);
};

TrayDisplayTest::TrayDisplayTest() : tray_(NULL), tray_display_(NULL) {
}

TrayDisplayTest::~TrayDisplayTest() {
}

void TrayDisplayTest::SetUp() {
  ash::test::AshTestBase::SetUp();
  tray_ = Shell::GetPrimaryRootWindowController()->GetSystemTray();
  tray_display_ = new TrayDisplay(tray_);
  tray_->AddTrayItem(tray_display_);
}

void TrayDisplayTest::CloseNotification() {
  tray()->CloseNotificationBubbleForTest();
  tray_display_->HideNotificationView();
  RunAllPendingInMessageLoop();
}

bool TrayDisplayTest::IsDisplayVisibleInTray() {
  return tray_display_->default_view() &&
      tray_display_->default_view()->visible();
}

base::string16 TrayDisplayTest::GetTrayDisplayText() {
  return tray_display_->GetDefaultViewMessage();
}

base::string16 TrayDisplayTest::GetTrayDisplayTooltipText() {
  if (!tray_display_->default_view())
    return base::string16();

  base::string16 tooltip;
  if (!tray_display_->default_view()->GetTooltipText(gfx::Point(), &tooltip))
    return base::string16();
  return tooltip;
}

base::string16 TrayDisplayTest::GetDisplayNotificationText() {
  return tray_display_->current_message();
}

TEST_F(TrayDisplayTest, NoInternalDisplay) {
  UpdateDisplay("400x400");
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_FALSE(IsDisplayVisibleInTray());

  UpdateDisplay("400x400,200x200");
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());
  base::string16 expected = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL);
  EXPECT_EQ(expected, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText1(expected), GetTrayDisplayTooltipText());

  // mirroring
  Shell::GetInstance()->display_manager()->SetSoftwareMirroring(true);
  UpdateDisplay("400x400,200x200");
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());
  expected = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING_NO_INTERNAL);
  EXPECT_EQ(expected, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText1(expected), GetTrayDisplayTooltipText());
}

TEST_F(TrayDisplayTest, InternalDisplay) {
  UpdateDisplay("400x400");
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  gfx::Display::SetInternalDisplayId(display_manager->first_display_id());

  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_FALSE(IsDisplayVisibleInTray());

  // Extended
  UpdateDisplay("400x400,200x200");
  string16 expected = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetSecondDisplayName());
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());
  EXPECT_EQ(expected, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText1(expected), GetTrayDisplayTooltipText());

  // Mirroring
  display_manager->SetSoftwareMirroring(true);
  UpdateDisplay("400x400,200x200");
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());

  expected = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetMirroredDisplayName());
  EXPECT_EQ(expected, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText1(expected), GetTrayDisplayTooltipText());

  // TODO(mukai): add test case for docked mode here.
}

TEST_F(TrayDisplayTest, InternalDisplayResized) {
  UpdateDisplay("400x400@1.5");
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  gfx::Display::SetInternalDisplayId(display_manager->first_display_id());

  // Shows the tray_display even though there's a single-display.
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());
  base::string16 internal_info = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_SINGLE_DISPLAY,
      GetFirstDisplayName(),
      UTF8ToUTF16("600x600"));
  EXPECT_EQ(internal_info, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText2(internal_info), GetTrayDisplayTooltipText());

  // Extended
  UpdateDisplay("400x400@1.5,200x200");
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());
  base::string16 expected = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetSecondDisplayName());
  EXPECT_EQ(expected, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText(expected, internal_info),
            GetTrayDisplayTooltipText());

  // Mirroring
  display_manager->SetSoftwareMirroring(true);
  UpdateDisplay("400x400@1.5,200x200");
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());
  expected = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetMirroredDisplayName());
  EXPECT_EQ(expected, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText(expected, internal_info),
            GetTrayDisplayTooltipText());
}

TEST_F(TrayDisplayTest, ExternalDisplayResized) {
  UpdateDisplay("400x400");
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  gfx::Display::SetInternalDisplayId(display_manager->first_display_id());

  // Shows the tray_display even though there's a single-display.
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_FALSE(IsDisplayVisibleInTray());

  // Extended
  UpdateDisplay("400x400,200x200@1.5");
  const gfx::Display& secondary_display = ScreenAsh::GetSecondaryDisplay();
  base::string16 secondary_annotation = UTF8ToUTF16(
      " (" + secondary_display.size().ToString() + ")");

  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());
  base::string16 expected = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED,
      GetSecondDisplayName() + secondary_annotation);
  EXPECT_EQ(expected, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText1(expected), GetTrayDisplayTooltipText());

  // Mirroring: in mirroring, it's not possible to lookup the DisplayInfo.
  display_manager->SetSoftwareMirroring(true);
  UpdateDisplay("400x400,200x200@1.5");
  tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
  EXPECT_TRUE(IsDisplayVisibleInTray());
  expected = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetMirroredDisplayName());
  EXPECT_EQ(expected, GetTrayDisplayText());
  EXPECT_EQ(GetTooltipText1(expected), GetTrayDisplayTooltipText());
}

TEST_F(TrayDisplayTest, DisplayNotifications) {
  test::TestSystemTrayDelegate* tray_delegate =
      static_cast<test::TestSystemTrayDelegate*>(
          Shell::GetInstance()->system_tray_delegate());
  tray_delegate->set_should_show_display_notification(true);

  UpdateDisplay("400x400");
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  gfx::Display::SetInternalDisplayId(display_manager->first_display_id());
  EXPECT_FALSE(tray()->HasNotificationBubble());

  // rotation.
  UpdateDisplay("400x400/r");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  base::string16 rotation_message = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetFirstDisplayName());
  EXPECT_EQ(rotation_message, GetDisplayNotificationText());

  CloseNotification();
  UpdateDisplay("400x400");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  EXPECT_EQ(rotation_message, GetDisplayNotificationText());

  // UI-scale
  CloseNotification();
  UpdateDisplay("400x400@1.5");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
          GetFirstDisplayName(), UTF8ToUTF16("600x600")),
      GetDisplayNotificationText());

  // UI-scale to 1.0
  CloseNotification();
  UpdateDisplay("400x400");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
          GetFirstDisplayName(), UTF8ToUTF16("400x400")),
      GetDisplayNotificationText());

  // No-update
  CloseNotification();
  UpdateDisplay("400x400");
  EXPECT_FALSE(tray()->HasNotificationBubble());

  // Extended.
  CloseNotification();
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetSecondDisplayName()),
      GetDisplayNotificationText());

  // Mirroring.
  CloseNotification();
  display_manager->SetSoftwareMirroring(true);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetMirroredDisplayName()),
      GetDisplayNotificationText());

  // Back to extended.
  CloseNotification();
  display_manager->SetSoftwareMirroring(false);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetSecondDisplayName()),
      GetDisplayNotificationText());

  // Resize the first display.
  UpdateDisplay("400x400@1.5,200x200");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
          GetFirstDisplayName(), UTF8ToUTF16("600x600")),
      GetDisplayNotificationText());

  // rotate the second.
  UpdateDisplay("400x400@1.5,200x200/r");
  EXPECT_TRUE(tray()->HasNotificationBubble());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetSecondDisplayName()),
      GetDisplayNotificationText());
}

}  // namespace internal
}  // namespace ash
