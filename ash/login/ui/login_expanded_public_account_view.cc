// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_expanded_public_account_view.h"

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr const char kLoginExpandedPublicAccountViewClassName[] =
    "LoginExpandedPublicAccountView";
constexpr int kExpandedViewWidthDp = 600;
constexpr int kExpandedViewHeightDp = 324;

constexpr int kTextLineHeightDp = 16;
constexpr int kRoundRectCornerRadiusDp = 2;
constexpr int kBorderThicknessDp = 1;
constexpr int kRightPaneMarginDp = 28;
constexpr int kLabelMarginDp = 20;
constexpr int kLeftMarginForSelectionButton = 8;
constexpr int kRightMarginForSelectionButton = 3;

constexpr SkColor kPublicSessionBackgroundColor =
    SkColorSetARGB(0xAB, 0x00, 0x00, 0x00);
constexpr SkColor kBorderColor = SkColorSetA(SK_ColorWHITE, 0x33);
constexpr SkColor kPublicSessionBlueColor =
    SkColorSetARGB(0xDE, 0x7B, 0xAA, 0xF7);
constexpr SkColor kSelectionMenuTitleColor =
    SkColorSetARGB(0x57, 0xFF, 0xFF, 0xFF);
constexpr SkColor kArrowButtonColor = SkColorSetARGB(0xFF, 0x42, 0x85, 0xF4);

constexpr int kDropDownIconSizeDp = 16;
constexpr int kArrowButtonSizeDp = 40;
constexpr int kAdvancedViewButtonWidthDp = 132;
constexpr int kAdvancedViewButtonHeightDp = 16;
constexpr int kSelectionBoxWidthDp = 178;
constexpr int kSelectionBoxHeightDp = 28;
constexpr int kTopSpacingForLabelInAdvancedViewDp = 7;
constexpr int kTopSpacingForLabelInRegularViewDp = 65;
constexpr int kSpacingBetweenLabelsDp = 17;
constexpr int kSpacingBetweenSelectionMenusDp = 15;
constexpr int kSpacingBetweenSelectionTitleAndButtonDp = 3;
constexpr int kSpacingBetweenLanguageMenuAndAdvancedViewButtonDp = 34;
constexpr int kSpacingBetweenAdvancedViewButtonAndLabelDp = 32;
constexpr int kTopSpacingForUserViewDp = 62;

constexpr int kNonEmptyWidth = 1;
constexpr int kNonEmptyHeight = 1;

views::Label* CreateLabel(const base::string16& text, SkColor color) {
  auto* label = new views::Label(text);
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(views::Label::GetDefaultFontList().Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(color);
  return label;
}

}  // namespace

// Button with text on the left side and an icon on the right side.
class SelectionButtonView : public views::Button {
 public:
  SelectionButtonView(const base::string16& text,
                      views::ButtonListener* listener)
      : views::Button(listener) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetLayoutManager(std::make_unique<views::FillLayout>());

    auto add_horizontal_margin = [&](int width,
                                     views::View* parent) -> views::View* {
      auto* margin = new NonAccessibleView();
      margin->SetPreferredSize(gfx::Size(width, kNonEmptyHeight));
      parent->AddChildView(margin);
      return margin;
    };

    auto* label_container = new NonAccessibleView();
    views::BoxLayout* label_layout = label_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
    label_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
    AddChildView(label_container);

    label_ = CreateLabel(text, SK_ColorWHITE);
    left_margin_view_ = add_horizontal_margin(left_margin_, label_container);
    label_container->AddChildView(label_);

    auto* icon_container = new NonAccessibleView();
    views::BoxLayout* icon_layout = icon_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
    icon_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
    AddChildView(icon_container);

    icon_ = new views::ImageView;
    icon_->SetVerticalAlignment(views::ImageView::CENTER);
    icon_->SetPreferredSize(
        gfx::Size(kDropDownIconSizeDp, kDropDownIconSizeDp));

    icon_container->AddChildView(icon_);
    right_margin_view_ = add_horizontal_margin(right_margin_, icon_container);
  }

  ~SelectionButtonView() override = default;

  // Return the preferred height of this view. This overrides the default
  // behavior in FillLayout::GetPreferredHeightForWidth which caluculates the
  // height based on its child height.
  int GetHeightForWidth(int w) const override {
    return GetPreferredSize().height();
  }

  void SetMargins(int left, int right) {
    if (left_margin_ == left && right_margin_ == right)
      return;

    left_margin_ = left;
    right_margin_ = right;
    left_margin_view_->SetPreferredSize(
        gfx::Size(left_margin_, kNonEmptyHeight));
    right_margin_view_->SetPreferredSize(
        gfx::Size(right_margin_, kNonEmptyHeight));
    Layout();
  }

  void SetTextColor(SkColor color) { label_->SetEnabledColor(color); }

  void SetIcon(const gfx::VectorIcon& icon, SkColor color) {
    icon_->SetImage(gfx::CreateVectorIcon(icon, color));
  }

 private:
  int left_margin_ = 0;
  int right_margin_ = 0;
  views::Label* label_ = nullptr;
  views::ImageView* icon_ = nullptr;
  views::View* left_margin_view_ = nullptr;
  views::View* right_margin_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SelectionButtonView);
};

// Implements the right part of the expanded public session view.
class RightPaneView : public NonAccessibleView,
                      public views::ButtonListener,
                      public views::StyledLabelListener {
 public:
  RightPaneView() {
    SetPreferredSize(
        gfx::Size(kExpandedViewWidthDp / 2, kExpandedViewHeightDp));
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kRightPaneMarginDp)));

    // Create labels view.
    labels_view_ = new NonAccessibleView();
    labels_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(), kSpacingBetweenLabelsDp));
    AddChildView(labels_view_);

    views::Label* top_label =
        CreateLabel(l10n_util::GetStringUTF16(
                        IDS_ASH_LOGIN_PUBLIC_ACCOUNT_MONITORING_WARNING),
                    SK_ColorWHITE);
    top_label->SetMultiLine(true);
    top_label->SetLineHeight(kTextLineHeightDp);

    const base::string16 link = l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE);
    size_t offset;
    const base::string16 text = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_PUBLIC_ACCOUNT_SIGNOUT_REMINDER, link, &offset);
    views::StyledLabel* bottom_label = new views::StyledLabel(text, this);

    views::StyledLabel::RangeStyleInfo style;
    style.custom_font = bottom_label->GetDefaultFontList().Derive(
        0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL);
    style.override_color = SK_ColorWHITE;
    bottom_label->AddStyleRange(gfx::Range(0, offset), style);

    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink();
    link_style.override_color = kPublicSessionBlueColor;
    bottom_label->AddStyleRange(gfx::Range(offset, offset + link.length()),
                                link_style);
    bottom_label->set_auto_color_readability_enabled(false);

    labels_view_->AddChildView(top_label);
    labels_view_->AddChildView(bottom_label);

    // Create button to show/hide advanced view.
    advanced_view_button_ = new SelectionButtonView(
        l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_PUBLIC_SESSION_LANGUAGE_AND_INPUT),
        this);
    advanced_view_button_->SetTextColor(kPublicSessionBlueColor);
    advanced_view_button_->SetIcon(kLoginScreenButtonDropdownIcon,
                                   kPublicSessionBlueColor);
    advanced_view_button_->SetPreferredSize(
        gfx::Size(kAdvancedViewButtonWidthDp, kAdvancedViewButtonHeightDp));
    AddChildView(advanced_view_button_);

    // Create advanced view.
    advanced_view_ = new NonAccessibleView();
    advanced_view_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
    AddChildView(advanced_view_);

    // Creates button to open the menu.
    auto create_menu_button =
        [&](const base::string16& text) -> SelectionButtonView* {
      auto* button = new SelectionButtonView(text, this);
      button->SetPreferredSize(
          gfx::Size(kSelectionBoxWidthDp, kSelectionBoxHeightDp));
      button->SetMargins(kLeftMarginForSelectionButton,
                         kRightMarginForSelectionButton);
      button->SetBorder(views::CreateRoundedRectBorder(
          kBorderThicknessDp, kRoundRectCornerRadiusDp, kBorderColor));
      button->SetIcon(kLoginScreenMenuDropdownIcon, kSelectionMenuTitleColor);
      return button;
    };

    auto create_padding = [](int amount) -> views::View* {
      auto* padding = new NonAccessibleView();
      padding->SetPreferredSize(gfx::Size(kNonEmptyWidth, amount));
      return padding;
    };

    views::Label* language_title = CreateLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_LANGUAGE_SELECTION_SELECT),
        kSelectionMenuTitleColor);
    language_selection_ = create_menu_button(base::string16());

    views::Label* keyboard_title = CreateLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_KEYBOARD_SELECTION_SELECT),
        kSelectionMenuTitleColor);
    keyboard_selection_ = create_menu_button(base::string16());

    advanced_view_->AddChildView(language_title);
    advanced_view_->AddChildView(
        create_padding(kSpacingBetweenSelectionTitleAndButtonDp));
    advanced_view_->AddChildView(language_selection_);
    advanced_view_->AddChildView(
        create_padding(kSpacingBetweenSelectionMenusDp));
    advanced_view_->AddChildView(keyboard_title);
    advanced_view_->AddChildView(
        create_padding(kSpacingBetweenSelectionTitleAndButtonDp));
    advanced_view_->AddChildView(keyboard_selection_);

    submit_button_ = new ArrowButtonView(this, kArrowButtonSizeDp);
    submit_button_->SetBackgroundColor(kArrowButtonColor);
    AddChildView(submit_button_);
  }

  ~RightPaneView() override = default;

  void Layout() override {
    const gfx::Rect bounds = GetContentsBounds();
    // Place the labels.
    const int label_width = bounds.width() - kLabelMarginDp;
    labels_view_->SetBounds(
        bounds.x(),
        show_advanced_view_ ? kTopSpacingForLabelInAdvancedViewDp + bounds.y()
                            : kTopSpacingForLabelInRegularViewDp + bounds.y(),
        label_width, labels_view_->GetHeightForWidth(label_width));

    // Place the advanced_view_button_.
    advanced_view_button_->SizeToPreferredSize();
    advanced_view_button_->SetPosition(gfx::Point(
        bounds.x(),
        show_advanced_view_
            ? labels_view_->bounds().bottom() + kSpacingBetweenLabelsDp
            : labels_view_->bounds().bottom() +
                  kSpacingBetweenAdvancedViewButtonAndLabelDp));

    // Place the advanced view.
    advanced_view_->SetVisible(show_advanced_view_);
    if (show_advanced_view_) {
      advanced_view_->SizeToPreferredSize();
      advanced_view_->SetPosition(gfx::Point(
          bounds.x(), advanced_view_button_->bounds().bottom() +
                          kSpacingBetweenLanguageMenuAndAdvancedViewButtonDp));
    }

    // Place the submit button.
    submit_button_->SizeToPreferredSize();
    submit_button_->SetPosition(
        gfx::Point(bounds.right() - kArrowButtonSizeDp,
                   bounds.bottom() - kArrowButtonSizeDp));
  }

  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == advanced_view_button_) {
      show_advanced_view_ = !show_advanced_view_;
      Layout();
    } else if (sender == submit_button_) {
      Shell::Get()->login_screen_controller()->LaunchPublicSession(
          current_user_->basic_user_info->account_id, selected_locale_,
          selected_keyboard_);
    }

    // TODO(crbug.com/809635): Show language and keyboard menu when clicks on
    // language_selection_ and keyboard_selection_.
  }

  // "Learn more" is clicked to show additional information of what the device
  // admin may monitor.
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override {
    NOTIMPLEMENTED();
  }

  void UpdateForUser(const mojom::LoginUserInfoPtr& user) {
    DCHECK_EQ(user->basic_user_info->type,
              user_manager::USER_TYPE_PUBLIC_ACCOUNT);
    current_user_ = user->Clone();
    if (selected_locale_.empty())
      selected_locale_ = user->public_account_info->default_locale;

    show_advanced_view_ = user->public_account_info->show_advanced_view;
    Layout();

    // TODO(crbug.com/809635): pre-populate keyboard list in LoginUserInfoPtr.
  }

  SelectionButtonView* advanced_view_button() { return advanced_view_button_; }
  ArrowButtonView* submit_button() { return submit_button_; }
  views::View* advanced_view() { return advanced_view_; }

 private:
  bool show_advanced_view_ = false;
  mojom::LoginUserInfoPtr current_user_;
  std::string selected_locale_;
  std::string selected_keyboard_;

  views::View* labels_view_ = nullptr;
  SelectionButtonView* advanced_view_button_ = nullptr;
  views::View* advanced_view_ = nullptr;
  SelectionButtonView* language_selection_ = nullptr;
  SelectionButtonView* keyboard_selection_ = nullptr;
  ArrowButtonView* submit_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RightPaneView);
};

LoginExpandedPublicAccountView::TestApi::TestApi(
    LoginExpandedPublicAccountView* view)
    : view_(view) {}

LoginExpandedPublicAccountView::TestApi::~TestApi() = default;

views::View* LoginExpandedPublicAccountView::TestApi::advanced_view_button() {
  return view_->right_pane_->advanced_view_button();
}

ArrowButtonView* LoginExpandedPublicAccountView::TestApi::submit_button() {
  return view_->right_pane_->submit_button();
}

views::View* LoginExpandedPublicAccountView::TestApi::advanced_view() {
  return view_->right_pane_->advanced_view();
}

LoginExpandedPublicAccountView::LoginExpandedPublicAccountView(
    const OnPublicSessionViewDismissed& on_dismissed)
    : NonAccessibleView(kLoginExpandedPublicAccountViewClassName),
      on_dismissed_(on_dismissed) {
  Shell::Get()->AddPreTargetHandler(this);
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
  SetPreferredSize(gfx::Size(kExpandedViewWidthDp, kExpandedViewHeightDp));

  user_view_ = new LoginUserView(
      LoginDisplayStyle::kLarge, false /*show_dropdown*/, true /*show_domain*/,
      base::DoNothing(), base::RepeatingClosure(), base::RepeatingClosure());
  user_view_->SetForceOpaque(true);
  user_view_->SetTapEnabled(false);

  auto* left_pane = new NonAccessibleView();
  AddChildView(left_pane);
  left_pane->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  left_pane->SetPreferredSize(
      gfx::Size(kExpandedViewWidthDp / 2, kExpandedViewHeightDp));

  auto* top_spacing = new NonAccessibleView();
  top_spacing->SetPreferredSize(
      gfx::Size(kNonEmptyWidth, kTopSpacingForUserViewDp));
  left_pane->AddChildView(top_spacing);
  left_pane->AddChildView(user_view_);
  left_pane->SetBorder(
      views::CreateSolidSidedBorder(0, 0, 0, kBorderThicknessDp, kBorderColor));

  right_pane_ = new RightPaneView();
  AddChildView(right_pane_);
}

LoginExpandedPublicAccountView::~LoginExpandedPublicAccountView() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void LoginExpandedPublicAccountView::ProcessPressedEvent(
    const ui::LocatedEvent* event) {
  if (!visible())
    return;

  if (GetBoundsInScreen().Contains(event->root_location()))
    return;

  SetVisible(false);
  on_dismissed_.Run();
}

void LoginExpandedPublicAccountView::UpdateForUser(
    const mojom::LoginUserInfoPtr& user) {
  user_view_->UpdateForUser(user, false /*animate*/);
  right_pane_->UpdateForUser(user);
}

const mojom::LoginUserInfoPtr& LoginExpandedPublicAccountView::current_user()
    const {
  return user_view_->current_user();
}

void LoginExpandedPublicAccountView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(kPublicSessionBackgroundColor);
  flags.setAntiAlias(true);
  canvas->DrawRoundRect(GetContentsBounds(), kRoundRectCornerRadiusDp, flags);
}

void LoginExpandedPublicAccountView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(event->AsLocatedEvent());
}

void LoginExpandedPublicAccountView::OnGestureEvent(ui::GestureEvent* event) {
  if ((event->type() == ui::ET_GESTURE_TAP ||
       event->type() == ui::ET_GESTURE_TAP_DOWN)) {
    ProcessPressedEvent(event->AsLocatedEvent());
  }
}

void LoginExpandedPublicAccountView::OnKeyEvent(ui::KeyEvent* event) {
  if (!visible() || event->type() != ui::ET_KEY_PRESSED)
    return;

  if (event->key_code() == ui::KeyboardCode::VKEY_ESCAPE) {
    SetVisible(false);
    on_dismissed_.Run();
  }
}

}  // namespace ash
