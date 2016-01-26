// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

using views::Widget;

namespace {

const int kWidth = 500;

class TestLayoutDelegate : public OpaqueBrowserFrameViewLayoutDelegate {
 public:
  enum WindowState {
    STATE_NORMAL,
    STATE_MAXIMIZED,
    STATE_MINIMIZED,
    STATE_FULLSCREEN
  };

  TestLayoutDelegate()
      : show_avatar_(false),
        show_caption_buttons_(true),
        window_state_(STATE_NORMAL) {
  }

  ~TestLayoutDelegate() override {}

  void SetWindowTitle(const base::string16& title) {
    window_title_ = title;
  }

  void SetShouldShowAvatar(bool show_avatar) {
    show_avatar_ = show_avatar;
  }

  void SetShouldShowCaptionButtons(bool show_caption_buttons) {
    show_caption_buttons_ = show_caption_buttons;
  }

  void SetWindowState(WindowState state) {
    window_state_ = state;
  }

  // OpaqueBrowserFrameViewLayoutDelegate overrides:

  bool ShouldShowWindowIcon() const override { return !window_title_.empty(); }

  bool ShouldShowWindowTitle() const override { return !window_title_.empty(); }

  base::string16 GetWindowTitle() const override { return window_title_; }

  int GetIconSize() const override {
    // The value on linux_aura and non-aura windows.
    return 17;
  }

  gfx::Size GetBrowserViewMinimumSize() const override {
    // Taken from a calculation in BrowserViewLayout.
    return gfx::Size(168, 64);
  }

  bool ShouldShowCaptionButtons() const override {
    return show_caption_buttons_;
  }

  bool ShouldShowAvatar() const override { return show_avatar_; }

  bool IsRegularOrGuestSession() const override { return true; }

  gfx::ImageSkia GetOTRAvatarIcon() const override {
    // The calculations depend on the size of the OTR resource, and chromeos
    // uses a different sized image, so hard code the size of the current
    // windows/linux one.
    gfx::ImageSkiaRep rep(gfx::Size(40, 29), 1.0f);
    gfx::ImageSkia image(rep);
    return image;
  }

  bool IsMaximized() const override { return window_state_ == STATE_MAXIMIZED; }

  bool IsMinimized() const override { return window_state_ == STATE_MINIMIZED; }

  bool IsFullscreen() const override {
    return window_state_ == STATE_FULLSCREEN;
  }

  bool IsTabStripVisible() const override { return window_title_.empty(); }

  int GetTabStripHeight() const override {
    return IsTabStripVisible() ? Tab::GetMinimumInactiveSize().height() : 0;
  }

  bool IsToolbarVisible() const override { return true; }

  gfx::Size GetTabstripPreferredSize() const override {
    // Measured from Tabstrip::GetPreferredSize().
    return IsTabStripVisible() ? gfx::Size(78, 29) : gfx::Size(0, 0);
  }

  int GetToolbarLeadingCornerClientWidth() const override {
    return 0;
  }

 private:
  base::string16 window_title_;
  bool show_avatar_;
  bool show_caption_buttons_;
  WindowState window_state_;

  DISALLOW_COPY_AND_ASSIGN(TestLayoutDelegate);
};

}  // namespace

class OpaqueBrowserFrameViewLayoutTest : public views::ViewsTestBase {
 public:
  OpaqueBrowserFrameViewLayoutTest() {}
  ~OpaqueBrowserFrameViewLayoutTest() override {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    delegate_.reset(new TestLayoutDelegate);
    layout_manager_ = new OpaqueBrowserFrameViewLayout(delegate_.get());
    layout_manager_->set_extra_caption_y(0);
    layout_manager_->set_window_caption_spacing(0);
    widget_ = new Widget;
    widget_->Init(CreateParams(Widget::InitParams::TYPE_POPUP));
    root_view_ = widget_->GetRootView();
    root_view_->SetSize(gfx::Size(kWidth, kWidth));
    root_view_->SetLayoutManager(layout_manager_);

    // Add the caption buttons. We use fake images because we're modeling the
    // Windows assets here, while the linux version uses differently sized
    // assets.
    //
    // TODO(erg): In a follow up patch, separate these sizes out into virtual
    // accessors so we can test both the windows and linux behaviours once we
    // start modifying the code.
    minimize_button_ = InitWindowCaptionButton(
        VIEW_ID_MINIMIZE_BUTTON, gfx::Size(26, 18));
    maximize_button_ = InitWindowCaptionButton(
        VIEW_ID_MAXIMIZE_BUTTON, gfx::Size(25, 18));
    restore_button_ = InitWindowCaptionButton(
        VIEW_ID_RESTORE_BUTTON, gfx::Size(25, 18));
    close_button_ = InitWindowCaptionButton(
        VIEW_ID_CLOSE_BUTTON, gfx::Size(43, 18));
  }

  void TearDown() override {
    widget_->CloseNow();

    views::ViewsTestBase::TearDown();
  }

 protected:
  views::ImageButton* InitWindowCaptionButton(ViewID view_id,
                                              const gfx::Size& size) {
    views::ImageButton* button = new views::ImageButton(nullptr);
    gfx::ImageSkiaRep rep(size, 1.0f);
    gfx::ImageSkia image(rep);
    button->SetImage(views::CustomButton::STATE_NORMAL, &image);
    button->set_id(view_id);
    root_view_->AddChildView(button);
    return button;
  }

  void AddWindowTitleIcons() {
    tab_icon_view_ = new TabIconView(nullptr, nullptr);
    tab_icon_view_->set_is_light(true);
    tab_icon_view_->set_id(VIEW_ID_WINDOW_ICON);
    root_view_->AddChildView(tab_icon_view_);

    window_title_ = new views::Label(delegate_->GetWindowTitle());
    window_title_->SetVisible(delegate_->ShouldShowWindowTitle());
    window_title_->SetEnabledColor(SK_ColorWHITE);
    window_title_->SetSubpixelRenderingEnabled(false);
    window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    window_title_->set_id(VIEW_ID_WINDOW_TITLE);
    root_view_->AddChildView(window_title_);
  }

  void AddNewAvatarButton() {
    new_avatar_button_ =
        new views::MenuButton(nullptr, base::string16(), nullptr, false);
    new_avatar_button_->set_id(VIEW_ID_NEW_AVATAR_BUTTON);
    root_view_->AddChildView(new_avatar_button_);
  }

  void ExpectBasicWindowBounds() {
    EXPECT_EQ("428,1 25x18", maximize_button_->bounds().ToString());
    EXPECT_EQ("402,1 26x18", minimize_button_->bounds().ToString());
    EXPECT_EQ("0,0 0x0", restore_button_->bounds().ToString());
    EXPECT_EQ("453,1 43x18", close_button_->bounds().ToString());
  }

  Widget* widget_;
  views::View* root_view_;
  OpaqueBrowserFrameViewLayout* layout_manager_;
  scoped_ptr<TestLayoutDelegate> delegate_;

  // Widgets:
  views::ImageButton* minimize_button_;
  views::ImageButton* maximize_button_;
  views::ImageButton* restore_button_;
  views::ImageButton* close_button_;

  TabIconView* tab_icon_view_;
  views::Label* window_title_;

  AvatarMenuButton* menu_button_;
  views::MenuButton* new_avatar_button_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueBrowserFrameViewLayoutTest);
};

TEST_F(OpaqueBrowserFrameViewLayoutTest, BasicWindow) {
  // Tests the layout of a default chrome window with no avatars, no window
  // titles, and a tabstrip.
  root_view_->Layout();

  ExpectBasicWindowBounds();

  // After some visual inspection, it really does look like the tabstrip is
  // initally positioned out of our view.
  EXPECT_EQ("-1,13 398x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("261x73", layout_manager_->GetMinimumSize(kWidth).ToString());

  // A normal window with no window icon still produces icon bounds for
  // Windows, which has a hidden icon that a user can double click on to close
  // the window.
  EXPECT_EQ("6,4 17x17", layout_manager_->IconBounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, BasicWindowMaximized) {
  // Tests the layout of a default chrome window with no avatars, no window
  // titles, and a tabstrip, but maximized this time.
  delegate_->SetWindowState(TestLayoutDelegate::STATE_MAXIMIZED);
  root_view_->Layout();

  // Note how the bounds start at the exact top of the window while maximized
  // while they start 1 pixel below when unmaximized.
  EXPECT_EQ("0,0 0x0", maximize_button_->bounds().ToString());
  EXPECT_EQ("403,0 26x18", minimize_button_->bounds().ToString());
  EXPECT_EQ("429,0 25x18", restore_button_->bounds().ToString());
  EXPECT_EQ("454,0 46x18", close_button_->bounds().ToString());

  EXPECT_EQ("-6,-3 393x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("262x61", layout_manager_->GetMinimumSize(kWidth).ToString());

  // In the maximized case, OpaqueBrowserFrameView::NonClientHitTest() uses
  // this rect, extended to the top left corner of the window.
  EXPECT_EQ("2,0 17x17", layout_manager_->IconBounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, MaximizedWithYOffset) {
  // Tests the layout of a basic chrome window with the caption buttons slightly
  // offset from the top of the screen (as they are on Linux).
  layout_manager_->set_extra_caption_y(2);
  delegate_->SetWindowState(TestLayoutDelegate::STATE_MAXIMIZED);
  root_view_->Layout();

  // Note how the bounds start at the exact top of the window, DESPITE the
  // caption Y offset of 2. This ensures that we obey Fitts' Law (the buttons
  // are clickable on the top edge of the screen). However, the buttons are 2
  // pixels taller, so the images appear to be offset by 2 pixels.
  EXPECT_EQ("0,0 0x0", maximize_button_->bounds().ToString());
  EXPECT_EQ("403,0 26x20", minimize_button_->bounds().ToString());
  EXPECT_EQ("429,0 25x20", restore_button_->bounds().ToString());
  EXPECT_EQ("454,0 46x20", close_button_->bounds().ToString());

  EXPECT_EQ("-6,-3 393x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("262x61", layout_manager_->GetMinimumSize(kWidth).ToString());

  // In the maximized case, OpaqueBrowserFrameView::NonClientHitTest() uses
  // this rect, extended to the top left corner of the window.
  EXPECT_EQ("2,0 17x17", layout_manager_->IconBounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, WindowButtonsOnLeft) {
  // Tests the layout of a chrome window with caption buttons on the left.
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  leading_buttons.push_back(views::FRAME_BUTTON_CLOSE);
  leading_buttons.push_back(views::FRAME_BUTTON_MINIMIZE);
  leading_buttons.push_back(views::FRAME_BUTTON_MAXIMIZE);
  layout_manager_->SetButtonOrdering(leading_buttons, trailing_buttons);
  root_view_->Layout();

  EXPECT_EQ("73,1 25x18", maximize_button_->bounds().ToString());
  EXPECT_EQ("47,1 26x18", minimize_button_->bounds().ToString());
  EXPECT_EQ("0,0 0x0", restore_button_->bounds().ToString());
  EXPECT_EQ("4,1 43x18", close_button_->bounds().ToString());

  EXPECT_EQ("92,13 398x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("261x73", layout_manager_->GetMinimumSize(kWidth).ToString());

  // If the buttons are on the left, there should be no hidden icon for the user
  // to double click.
  EXPECT_EQ("0,0 0x0", layout_manager_->IconBounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, WithoutCaptionButtons) {
  // Tests the layout of a default chrome window with no caption buttons (which
  // should force the tab strip to be condensed).
  delegate_->SetShouldShowCaptionButtons(false);
  root_view_->Layout();

  EXPECT_EQ("0,0 0x0", maximize_button_->bounds().ToString());
  EXPECT_EQ("0,0 0x0", minimize_button_->bounds().ToString());
  EXPECT_EQ("0,0 0x0", restore_button_->bounds().ToString());
  EXPECT_EQ("0,0 0x0", close_button_->bounds().ToString());

  EXPECT_EQ("-6,-3 501x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("251x61", layout_manager_->GetMinimumSize(kWidth).ToString());

  // A normal window with no window icon still produces icon bounds for
  // Windows, which has a hidden icon that a user can double click on to close
  // the window.
  EXPECT_EQ("2,0 17x17", layout_manager_->IconBounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, MaximizedWithoutCaptionButtons) {
  // Tests the layout of a maximized chrome window with no caption buttons.
  delegate_->SetWindowState(TestLayoutDelegate::STATE_MAXIMIZED);
  delegate_->SetShouldShowCaptionButtons(false);
  root_view_->Layout();

  EXPECT_EQ("0,0 0x0", maximize_button_->bounds().ToString());
  EXPECT_EQ("0,0 0x0", minimize_button_->bounds().ToString());
  EXPECT_EQ("0,0 0x0", restore_button_->bounds().ToString());
  EXPECT_EQ("0,0 0x0", close_button_->bounds().ToString());

  EXPECT_EQ("-6,-3 501x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("251x61", layout_manager_->GetMinimumSize(kWidth).ToString());

  // In the maximized case, OpaqueBrowserFrameView::NonClientHitTest() uses
  // this rect, extended to the top left corner of the window.
  EXPECT_EQ("2,0 17x17", layout_manager_->IconBounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, WithWindowTitleAndIcon) {
  // Tests the layout of pop up windows.
  delegate_->SetWindowTitle(base::ASCIIToUTF16("Window Title"));
  AddWindowTitleIcons();
  root_view_->Layout();

  // We should have the right hand side should match the BasicWindow case.
  ExpectBasicWindowBounds();

  // Check the location of the tab icon and window title.
  EXPECT_EQ("6,3 17x17", tab_icon_view_->bounds().ToString());
  EXPECT_EQ("27,3 370x17", window_title_->bounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, WindowWithNewAvatar) {
  // Tests a normal tabstrip window with the new style avatar icon.
  AddNewAvatarButton();
  root_view_->Layout();

  ExpectBasicWindowBounds();

  // Check the location of the avatar button.
  EXPECT_EQ("385,1 12x18", new_avatar_button_->bounds().ToString());
  // The new tab button is 39px wide and slides completely under the new
  // avatar button, thus increasing the tabstrip by that amount.
  EXPECT_EQ("-1,13 420x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("261x73", layout_manager_->GetMinimumSize(kWidth).ToString());
}
