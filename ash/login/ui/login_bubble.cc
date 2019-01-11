// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_bubble.h"

#include <memory>
#include <utility>

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_menu_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_properties.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

constexpr char kLegacySupervisedUserManagementDisplayURL[] =
    "www.chrome.com/manage";

// Spacing between the child view inside the bubble view.
constexpr int kBubbleBetweenChildSpacingDp = 6;

// An alpha value for the sub message in the user menu.
constexpr SkAlpha kSubMessageColorAlpha = 0x89;

// Color of the "Remove user" text.
constexpr SkColor kRemoveUserInitialColor = gfx::kGoogleBlueDark400;
constexpr SkColor kRemoveUserConfirmColor = gfx::kGoogleRedDark500;

// Margin/inset of the entries for the user menu.
constexpr int kUserMenuMarginWidth = 14;
constexpr int kUserMenuMarginHeight = 16;
// Distance above/below the separator.
constexpr int kUserMenuMarginAroundSeparatorDp = 16;
// Distance between labels.
constexpr int kUserMenuVerticalDistanceBetweenLabelsDp = 16;
// Margin around remove user button.
constexpr int kUserMenuMarginAroundRemoveUserButtonDp = 4;

// Horizontal spacing with the anchor view.
constexpr int kAnchorViewUserMenuHorizontalSpacingDp = 98;

// Vertical spacing between the anchor view and user menu.
constexpr int kAnchorViewUserMenuVerticalSpacingDp = 4;

// Maximum height per label line, so that the user menu dropdown will fit in the
// screen.
constexpr int kUserMenuLabelLineHeight = 20;

views::Label* CreateLabel(const base::string16& message, SkColor color) {
  views::Label* label = new views::Label(message, views::style::CONTEXT_LABEL,
                                         views::style::STYLE_PRIMARY);
  label->SetLineHeight(kUserMenuLabelLineHeight);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(color);
  label->SetSubpixelRenderingEnabled(false);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  label->SetFontList(base_font_list.Derive(0, gfx::Font::FontStyle::NORMAL,
                                           gfx::Font::Weight::NORMAL));
  return label;
}

// A button that holds a child view.
class ButtonWithContent : public views::Button {
 public:
  ButtonWithContent(views::ButtonListener* listener, views::View* content)
      : views::Button(listener) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(content);

    // Increase the size of the button so that the focus is not rendered next to
    // the text.
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kUserMenuMarginAroundRemoveUserButtonDp,
                    kUserMenuMarginAroundRemoveUserButtonDp)));
    SetFocusPainter(views::Painter::CreateSolidFocusPainter(
        kFocusBorderColor, kFocusBorderThickness, gfx::InsetsF()));
  }

  ~ButtonWithContent() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ButtonWithContent);
};

// A view that has a customizable accessible name.
class ViewWithAccessibleName : public views::View {
 public:
  ViewWithAccessibleName(const base::string16& accessible_name)
      : accessible_name_(accessible_name) {}
  ~ViewWithAccessibleName() override = default;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kStaticText;
    node_data->SetName(accessible_name_);
  }

 private:
  const base::string16 accessible_name_;
  DISALLOW_COPY_AND_ASSIGN(ViewWithAccessibleName);
};

class LoginUserMenuView : public LoginBaseBubbleView,
                          public views::ButtonListener {
 public:
  LoginUserMenuView(const base::string16& username,
                    const base::string16& email,
                    user_manager::UserType type,
                    bool is_owner,
                    views::View* anchor_view,
                    LoginButton* bubble_opener_,
                    bool show_remove_user,
                    base::OnceClosure on_remove_user_warning_shown,
                    base::OnceClosure on_remove_user_requested)
      : LoginBaseBubbleView(anchor_view),
        bubble_opener_(bubble_opener_),
        on_remove_user_warning_shown_(std::move(on_remove_user_warning_shown)),
        on_remove_user_requested_(std::move(on_remove_user_requested)) {
    set_parent_window(Shell::GetContainer(
        Shell::GetPrimaryRootWindow(), kShellWindowId_SettingBubbleContainer));
    // This view has content the user can interact with if the remove user
    // button is displayed.
    set_can_activate(show_remove_user);

    set_anchor_view_insets(gfx::Insets(kAnchorViewUserMenuVerticalSpacingDp,
                                       kAnchorViewUserMenuHorizontalSpacingDp));

    // LoginUserMenuView does not use the parent margins. Further, because the
    // splitter spans the entire view set_margins cannot be used.
    set_margins(gfx::Insets());
    // The bottom margin is less the margin around the remove user button, which
    // is always visible.
    gfx::Insets margins(
        kUserMenuMarginHeight, kUserMenuMarginWidth,
        kUserMenuMarginHeight - kUserMenuMarginAroundRemoveUserButtonDp,
        kUserMenuMarginWidth);
    auto setup_horizontal_margin_container = [&](views::View* container) {
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical,
          gfx::Insets(0, margins.left(), 0, margins.right())));
      AddChildView(container);
      return container;
    };

    // Add vertical whitespace.
    auto add_space = [](views::View* root, int amount) {
      auto* spacer = new NonAccessibleView("Whitespace");
      spacer->SetPreferredSize(gfx::Size(1, amount));
      root->AddChildView(spacer);
    };

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(margins.top(), 0, margins.bottom(), 0)));

    // User information.
    {
      base::string16 display_username =
          is_owner ? l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_POD_OWNER_USER,
                                                username)
                   : username;

      views::View* container = setup_horizontal_margin_container(
          new NonAccessibleView("UsernameLabel MarginContainer"));
      username_label_ = CreateLabel(display_username, SK_ColorWHITE);
      container->AddChildView(username_label_);
      add_space(container, kBubbleBetweenChildSpacingDp);
      views::Label* email_label =
          CreateLabel(email, SkColorSetA(SK_ColorWHITE, kSubMessageColorAlpha));
      container->AddChildView(email_label);
    }

    // Remove user.
    if (show_remove_user) {
      DCHECK(!is_owner);

      // Add separator.
      add_space(this, kUserMenuMarginAroundSeparatorDp);
      auto* separator = new views::Separator();
      separator->SetColor(SkColorSetA(SK_ColorWHITE, 0x2B));
      AddChildView(separator);
      // The space below the separator is less the margin around remove user;
      // this is readded if showing confirmation.
      add_space(this, kUserMenuMarginAroundSeparatorDp -
                          kUserMenuMarginAroundRemoveUserButtonDp);

      auto make_label = [this](const base::string16& text) {
        views::Label* label = CreateLabel(text, SK_ColorWHITE);
        label->SetMultiLine(true);
        label->SetAllowCharacterBreak(true);
        // Make sure to set a maximum label width, otherwise text wrapping will
        // significantly increase width and layout may not work correctly if
        // the input string is very long.
        label->SetMaximumWidth(GetPreferredSize().width());
        return label;
      };

      base::string16 part1 = l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_1);
      if (type == user_manager::UserType::USER_TYPE_SUPERVISED) {
        part1 = l10n_util::GetStringFUTF16(
            IDS_ASH_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
            base::UTF8ToUTF16(ash::kLegacySupervisedUserManagementDisplayURL));
      }
      base::string16 part2 = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2, email);

      remove_user_confirm_data_ = setup_horizontal_margin_container(
          new ViewWithAccessibleName(part1 + base::ASCIIToUTF16(" ") + part2));
      remove_user_confirm_data_->SetVisible(false);

      // Account for margin that was removed below the separator for the add
      // user button.
      add_space(remove_user_confirm_data_,
                kUserMenuMarginAroundRemoveUserButtonDp);
      remove_user_confirm_data_->AddChildView(make_label(part1));
      add_space(remove_user_confirm_data_,
                kUserMenuVerticalDistanceBetweenLabelsDp);
      remove_user_confirm_data_->AddChildView(make_label(part2));
      // Reduce margin since the remove user button comes next.
      add_space(remove_user_confirm_data_,
                kUserMenuVerticalDistanceBetweenLabelsDp -
                    kUserMenuMarginAroundRemoveUserButtonDp);

      auto* container = setup_horizontal_margin_container(
          new NonAccessibleView("RemoveUserButton MarginContainer"));
      remove_user_label_ =
          CreateLabel(l10n_util::GetStringUTF16(
                          IDS_ASH_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME),
                      kRemoveUserInitialColor);
      remove_user_button_ = new ButtonWithContent(this, remove_user_label_);
      remove_user_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      remove_user_button_->set_id(
          LoginBubble::kUserMenuRemoveUserButtonIdForTest);
      remove_user_button_->SetAccessibleName(remove_user_label_->text());
      container->AddChildView(remove_user_button_);
    }
  }

  void RequestFocus() override {
    // This view has no actual interesting contents to focus, so immediately
    // forward to the button.
    if (remove_user_button_)
      remove_user_button_->RequestFocus();
  }

  void AddedToWidget() override {
    LoginBaseBubbleView::AddedToWidget();
    // Set up focus traversable parent so that keyboard focus can continue in
    // the lock window, otherwise focus will be trapped inside the bubble.
    if (GetAnchorView()) {
      GetWidget()->SetFocusTraversableParent(
          anchor_widget()->GetFocusTraversable());
      GetWidget()->SetFocusTraversableParentView(GetAnchorView());
    }
  }

  ~LoginUserMenuView() override = default;

  // LoginBaseBubbleView:
  LoginButton* GetBubbleOpener() const override { return bubble_opener_; }

  // views::View:
  const char* GetClassName() const override { return "LoginUserMenuView"; }
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = LoginBaseBubbleView::CalculatePreferredSize();
    // We don't use margins() directly which means that we need to account for
    // the margin width here. Margin height is accounted for by the layout code.
    size.Enlarge(kUserMenuMarginWidth, 0);
    return size;
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    // Show confirmation warning. The user has to click the button again before
    // we actually allow the exit.
    if (!remove_user_confirm_data_->visible()) {
      remove_user_confirm_data_->SetVisible(true);
      remove_user_label_->SetEnabledColor(kRemoveUserConfirmColor);
      SetSize(GetPreferredSize());
      SizeToContents();
      Layout();

      // Fire an accessibility alert to make ChromeVox read the warning message
      // and remove button.
      remove_user_confirm_data_->NotifyAccessibilityEvent(
          ax::mojom::Event::kAlert, true /*send_native_event*/);
      remove_user_button_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                                    true /*send_native_event*/);

      if (on_remove_user_warning_shown_)
        std::move(on_remove_user_warning_shown_).Run();
      return;
    }

    // Immediately hide the widget with no animation before running the remove
    // user callback. If an animation is triggered while the the views hierarchy
    // for this bubble is being torn down, we can get a crash.
    GetWidget()->Hide();

    if (on_remove_user_requested_)
      std::move(on_remove_user_requested_).Run();
  }

  views::View* remove_user_button() { return remove_user_button_; }

  views::View* remove_user_confirm_data() { return remove_user_confirm_data_; }

  views::Label* username_label() { return username_label_; }

 private:
  LoginButton* bubble_opener_ = nullptr;
  base::OnceClosure on_remove_user_warning_shown_;
  base::OnceClosure on_remove_user_requested_;
  views::View* remove_user_confirm_data_ = nullptr;
  views::Label* remove_user_label_ = nullptr;
  ButtonWithContent* remove_user_button_ = nullptr;
  views::Label* username_label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginUserMenuView);
};

}  // namespace

// static
const int LoginBubble::kUserMenuRemoveUserButtonIdForTest = 1;

LoginBubble::TestApi::TestApi(LoginBaseBubbleView* bubble_view)
    : bubble_view_(bubble_view) {}

views::View* LoginBubble::TestApi::user_menu_remove_user_button() {
  return static_cast<LoginUserMenuView*>(bubble_view_)->remove_user_button();
}

views::View* LoginBubble::TestApi::remove_user_confirm_data() {
  return static_cast<LoginUserMenuView*>(bubble_view_)
      ->remove_user_confirm_data();
}

views::Label* LoginBubble::TestApi::username_label() {
  return static_cast<LoginUserMenuView*>(bubble_view_)->username_label();
}

LoginBubble::LoginBubble() = default;

LoginBubble::~LoginBubble() = default;

void LoginBubble::ShowUserMenu(const base::string16& username,
                               const base::string16& email,
                               user_manager::UserType type,
                               bool is_owner,
                               views::View* anchor_view,
                               LoginButton* bubble_opener,
                               bool show_remove_user,
                               base::OnceClosure on_remove_user_warning_shown,
                               base::OnceClosure on_remove_user_requested) {
  if (bubble_view_)
    CloseImmediately();

  bubble_view_ = new LoginUserMenuView(
      username, email, type, is_owner, anchor_view, bubble_opener,
      show_remove_user, std::move(on_remove_user_warning_shown),
      std::move(on_remove_user_requested));
  // Prevent focus from going into |bubble_view_|.
  bool had_focus = bubble_view_->GetBubbleOpener() &&
                   bubble_view_->GetBubbleOpener()->HasFocus();
  bubble_view_->Show();
  if (had_focus) {
    // Try to focus the bubble view only if the bubble opener was focused.
    bubble_view_->RequestFocus();
  }
}

void LoginBubble::ShowSelectionMenu(LoginMenuView* menu) {
  if (bubble_view_)
    CloseImmediately();

  const bool had_focus =
      menu->GetBubbleOpener() && menu->GetBubbleOpener()->HasFocus();

  // Transfer the ownership of |menu| to bubble widget.
  bubble_view_ = menu;
  bubble_view_->Show();

  if (had_focus) {
    // Try to focus the bubble view only if the bubble opener was focused.
    bubble_view_->RequestFocus();
  }
}

void LoginBubble::Close() {
  if (bubble_view_)
    bubble_view_->Hide();
}

void LoginBubble::CloseImmediately() {
  DCHECK(bubble_view_);

  if (bubble_view_->GetWidget())
    bubble_view_->GetWidget()->Close();

  bubble_view_ = nullptr;
}

bool LoginBubble::IsVisible() {
  return bubble_view_ && bubble_view_->GetWidget()->IsVisible();
}

}  // namespace ash
