// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
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
        window_state_(STATE_NORMAL) {
  }

  virtual ~TestLayoutDelegate() {}

  void SetWindowTitle(const base::string16& title) {
    window_title_ = title;
  }

  void SetShouldShowAvatar(bool show_avatar) {
    show_avatar_ = show_avatar;
  }

  void SetWindowState(WindowState state) {
    window_state_ = state;
  }

  virtual bool ShouldShowWindowIcon() const OVERRIDE {
    return !window_title_.empty();
  }

  virtual bool ShouldShowWindowTitle() const OVERRIDE {
    return !window_title_.empty();
  }

  virtual base::string16 GetWindowTitle() const OVERRIDE {
    return window_title_;
  }

  virtual int GetIconSize() const OVERRIDE {
    // The value on linux_aura and non-aura windows.
    return 17;
  }

  virtual bool ShouldLeaveOffsetNearTopBorder() const OVERRIDE {
    return !IsMaximized();
  }

  virtual gfx::Size GetBrowserViewMinimumSize() const OVERRIDE {
    // Taken from a calculation in BrowserViewLayout.
    return gfx::Size(168, 64);
  }

  virtual bool ShouldShowAvatar() const OVERRIDE {
    return show_avatar_;
  }

  virtual gfx::ImageSkia GetOTRAvatarIcon() const OVERRIDE {
    // The calculations depend on the size of the OTR resource, and chromeos
    // uses a different sized image, so hard code the size of the current
    // windows/linux one.
    gfx::ImageSkiaRep rep(gfx::Size(40, 29), 1.0f);
    gfx::ImageSkia image(rep);
    return image;
  }

  virtual bool IsMaximized() const OVERRIDE {
    return window_state_ == STATE_MAXIMIZED;
  }

  virtual bool IsMinimized() const OVERRIDE {
    return window_state_ == STATE_MINIMIZED;
  }

  virtual bool IsFullscreen() const OVERRIDE {
    return window_state_ == STATE_FULLSCREEN;
  }

  virtual bool IsTabStripVisible() const OVERRIDE {
    return window_title_.empty();
  }

  virtual int GetTabStripHeight() const OVERRIDE {
    return IsTabStripVisible() ? Tab::GetMinimumUnselectedSize().height() : 0;
  }

  virtual int GetAdditionalReservedSpaceInTabStrip() const OVERRIDE {
    return 0;
  }

  virtual gfx::Size GetTabstripPreferredSize() const OVERRIDE {
    // Measured from Tabstrip::GetPreferredSize().
    return IsTabStripVisible() ? gfx::Size(78, 29) : gfx::Size(0, 0);
  }

 private:
  base::string16 window_title_;
  bool show_avatar_;
  WindowState window_state_;

  DISALLOW_COPY_AND_ASSIGN(TestLayoutDelegate);
};

}  // namespace

class OpaqueBrowserFrameViewLayoutTest : public views::ViewsTestBase {
 public:
  OpaqueBrowserFrameViewLayoutTest() {}
  virtual ~OpaqueBrowserFrameViewLayoutTest() {}

  virtual void SetUp() OVERRIDE {
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

  virtual void TearDown() OVERRIDE {
    widget_->CloseNow();

    views::ViewsTestBase::TearDown();
  }

 protected:
  views::ImageButton* InitWindowCaptionButton(ViewID view_id,
                                              const gfx::Size& size) {
    views::ImageButton* button = new views::ImageButton(NULL);
    gfx::ImageSkiaRep rep(size, 1.0f);
    gfx::ImageSkia image(rep);
    button->SetImage(views::CustomButton::STATE_NORMAL, &image);
    button->set_id(view_id);
    root_view_->AddChildView(button);
    return button;
  }

  void AddWindowTitleIcons() {
    tab_icon_view_ = new TabIconView(NULL);
    tab_icon_view_->set_is_light(true);
    tab_icon_view_->set_id(VIEW_ID_WINDOW_ICON);
    root_view_->AddChildView(tab_icon_view_);

    window_title_ = new views::Label(delegate_->GetWindowTitle(),
                                     default_font_);
    window_title_->SetVisible(delegate_->ShouldShowWindowTitle());
    window_title_->SetEnabledColor(SK_ColorWHITE);
    window_title_->SetBackgroundColor(0x00000000);
    window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    window_title_->set_id(VIEW_ID_WINDOW_TITLE);
    root_view_->AddChildView(window_title_);
  }

  void AddAvatarButton() {
    menu_button_ = new views::MenuButton(NULL, string16(), NULL, false);
    menu_button_->set_id(VIEW_ID_AVATAR_BUTTON);
    delegate_->SetShouldShowAvatar(true);
    root_view_->AddChildView(menu_button_);
  }

  void AddAvatarLabel() {
    avatar_label_ = new views::MenuButton(NULL, string16(), NULL, false);
    avatar_label_->set_id(VIEW_ID_AVATAR_LABEL);
    root_view_->AddChildView(avatar_label_);

    // The avatar label should only be used together with the avatar button.
    AddAvatarButton();
  }

  void ExpectBasicWindowBounds() {
    EXPECT_EQ("428,1 25x18", maximize_button_->bounds().ToString());
    EXPECT_EQ("402,1 26x18", minimize_button_->bounds().ToString());
    EXPECT_EQ("0,0 0x0", restore_button_->bounds().ToString());
    EXPECT_EQ("453,1 43x18", close_button_->bounds().ToString());
  }

  gfx::Font default_font_;

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

  views::MenuButton* menu_button_;
  views::MenuButton* avatar_label_;

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

  // Note how the bonds start at the exact top of the window while maximized
  // while they start 1 pixel below when unmaximized.
  EXPECT_EQ("0,0 0x0", maximize_button_->bounds().ToString());
  EXPECT_EQ("403,0 26x18", minimize_button_->bounds().ToString());
  EXPECT_EQ("429,0 25x18", restore_button_->bounds().ToString());
  EXPECT_EQ("454,0 46x18", close_button_->bounds().ToString());

  EXPECT_EQ("-5,-3 392x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("262x61", layout_manager_->GetMinimumSize(kWidth).ToString());

  // In the maximized case, OpaqueBrowserFrameView::NonClientHitTest() uses
  // this rect, extended to the top left corner of the window.
  EXPECT_EQ("2,0 17x17", layout_manager_->IconBounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, WithWindowTitleAndIcon) {
  // Tests the layout of pop up windows.
  delegate_->SetWindowTitle(ASCIIToUTF16("Window Title"));
  AddWindowTitleIcons();
  root_view_->Layout();

  // We should have the right hand side should match the BasicWindow case.
  ExpectBasicWindowBounds();

  // Check the location of the tab icon and window title.
  EXPECT_EQ("6,3 17x17", tab_icon_view_->bounds().ToString());
  EXPECT_EQ("27,3 370x17", window_title_->bounds().ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, WindowWithAvatar) {
  // Tests a normal tabstrip window with an avatar icon.
  AddAvatarButton();
  root_view_->Layout();

  ExpectBasicWindowBounds();

  // Check the location of the avatar
  EXPECT_EQ("7,11 40x29", menu_button_->bounds().ToString());
  EXPECT_EQ("45,13 352x29",
            layout_manager_->GetBoundsForTabStrip(
                delegate_->GetTabstripPreferredSize(), kWidth).ToString());
  EXPECT_EQ("261x73", layout_manager_->GetMinimumSize(kWidth).ToString());
}

TEST_F(OpaqueBrowserFrameViewLayoutTest, WindowWithAvatarLabelAndButton) {
  AddAvatarLabel();
  root_view_->Layout();

  ExpectBasicWindowBounds();

  // Check the location of the avatar label relative to the avatar button.
  // The label height and width depends on the font size and the text displayed.
  // This may possibly change, so we don't test it here.
  EXPECT_EQ(menu_button_->bounds().x() - 2, avatar_label_->bounds().x());
  EXPECT_EQ(
      menu_button_->bounds().bottom() - 3 - avatar_label_->bounds().height(),
      avatar_label_->bounds().y());
}
