// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/message_center/notifier_settings_view.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "ash/message_center/message_center_style.h"
#include "ash/message_center/message_center_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {

using message_center::MessageCenter;
using message_center::Notifier;
using message_center::NotifierGroup;
using message_center::NotifierId;
using message_center::NotifierSettingsProvider;

namespace {

const int kEntryHeight = 48;
const int kHorizontalMargin = 12;
const int kEntryIconSize = 20;
const int kInternalHorizontalSpacing = 16;
const int kSmallerInternalHorizontalSpacing = 12;
const int kCheckboxSizeWithPadding = 28;

// The width of the settings pane in pixels.
const int kWidth = 360;

// The width of the learn more icon in pixels.
const int kLearnMoreSize = 12;

// The width of the click target that contains the learn more button in pixels.
const int kLearnMoreTargetWidth = 28;

// The height of the click target that contains the learn more button in pixels.
const int kLearnMoreTargetHeight = 40;

// The minimum height of the settings pane in pixels.
const int kMinimumHeight = 480;

// Checkboxes have some built-in right padding blank space.
const int kInnateCheckboxRightPadding = 2;

// Spec defines the checkbox size; the innate padding throws this measurement
// off so we need to compute a slightly different area for the checkbox to
// inhabit.
constexpr int kComputedCheckboxSize =
    kCheckboxSizeWithPadding - kInnateCheckboxRightPadding;

// A function to create a focus border.
std::unique_ptr<views::Painter> CreateFocusPainter() {
  return views::Painter::CreateSolidFocusPainter(
      message_center::kFocusBorderColor, gfx::Insets(1, 2, 3, 2));
}

// TODO(tetsui): Give more general names and remove kEntryHeight, etc.
constexpr gfx::Insets kTopLabelPadding(16, 18, 15, 0);
const int kQuietModeViewSpacing = 18;

constexpr gfx::Insets kHeaderViewPadding(4, 0, 4, 0);
constexpr gfx::Insets kQuietModeViewPadding(16, 18, 15, 14);
constexpr SkColor kQuietModeIconColor = SkColorSetARGB(0x8A, 0x5A, 0x5A, 0x5A);
constexpr SkColor kTopLabelColor = SkColorSetRGB(0x42, 0x85, 0xF4);
constexpr SkColor kLabelColor = SkColorSetARGB(0xDE, 0x0, 0x0, 0x0);
const int kLabelFontSize = 13;

// EntryView ------------------------------------------------------------------

// The view to guarantee the 48px height and place the contents at the
// middle. It also guarantee the left margin.
class EntryView : public views::View {
 public:
  explicit EntryView(views::View* contents);
  ~EntryView() override;

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnBlur() override;

 private:
  std::unique_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(EntryView);
};

EntryView::EntryView(views::View* contents)
    : focus_painter_(CreateFocusPainter()) {
  AddChildView(contents);
}

EntryView::~EntryView() {}

void EntryView::Layout() {
  DCHECK_EQ(1, child_count());
  views::View* content = child_at(0);
  int content_width = width();
  int content_height = content->GetHeightForWidth(content_width);
  int y = std::max((height() - content_height) / 2, 0);
  content->SetBounds(0, y, content_width, content_height);
}

gfx::Size EntryView::CalculatePreferredSize() const {
  return gfx::Size(kWidth, kEntryHeight);
}

void EntryView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  DCHECK_EQ(1, child_count());
  child_at(0)->GetAccessibleNodeData(node_data);
}

void EntryView::OnFocus() {
  views::View::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
  // We render differently when focused.
  SchedulePaint();
}

bool EntryView::OnKeyPressed(const ui::KeyEvent& event) {
  return child_at(0)->OnKeyPressed(event);
}

bool EntryView::OnKeyReleased(const ui::KeyEvent& event) {
  return child_at(0)->OnKeyReleased(event);
}

void EntryView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void EntryView::OnBlur() {
  View::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

}  // namespace

// NotifierSettingsView::NotifierButton ---------------------------------------

// We do not use views::Checkbox class directly because it doesn't support
// showing 'icon'.
NotifierSettingsView::NotifierButton::NotifierButton(
    NotifierSettingsProvider* provider,
    std::unique_ptr<Notifier> notifier,
    views::ButtonListener* listener)
    : views::Button(listener),
      provider_(provider),
      notifier_(std::move(notifier)),
      icon_view_(new views::ImageView()),
      name_view_(new views::Label(notifier_->name)),
      checkbox_(new views::Checkbox(base::string16(), true /* force_md */)),
      learn_more_(nullptr) {
  DCHECK(provider_);
  DCHECK(notifier_);

  name_view_->SetAutoColorReadabilityEnabled(false);
  name_view_->SetEnabledColor(kLabelColor);
  // "Roboto-Regular, 13sp" is specified in the mock.
  name_view_->SetFontList(message_center_style::GetFontListForSizeAndWeight(
      kLabelFontSize, gfx::Font::Weight::NORMAL));

  checkbox_->SetChecked(notifier_->enabled);
  checkbox_->set_listener(this);
  checkbox_->SetFocusBehavior(FocusBehavior::NEVER);
  checkbox_->SetAccessibleName(notifier_->name);

  if (ShouldHaveLearnMoreButton()) {
    // Create a more-info button that will be right-aligned.
    learn_more_ = new views::ImageButton(this);
    learn_more_->SetFocusPainter(CreateFocusPainter());
    learn_more_->SetFocusForPlatform();

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    learn_more_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetImageSkiaNamed(IDR_NOTIFICATION_ADVANCED_SETTINGS));
    learn_more_->SetImage(
        views::Button::STATE_HOVERED,
        rb.GetImageSkiaNamed(IDR_NOTIFICATION_ADVANCED_SETTINGS_HOVER));
    learn_more_->SetImage(
        views::Button::STATE_PRESSED,
        rb.GetImageSkiaNamed(IDR_NOTIFICATION_ADVANCED_SETTINGS_PRESSED));
    learn_more_->SetState(views::Button::STATE_NORMAL);
    int learn_more_border_width = (kLearnMoreTargetWidth - kLearnMoreSize) / 2;
    int learn_more_border_height =
        (kLearnMoreTargetHeight - kLearnMoreSize) / 2;
    // The image itself is quite small, this large invisible border creates a
    // much bigger click target.
    learn_more_->SetBorder(views::CreateEmptyBorder(
        learn_more_border_height, learn_more_border_width,
        learn_more_border_height, learn_more_border_width));
    learn_more_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  }

  UpdateIconImage(notifier_->icon);
}

NotifierSettingsView::NotifierButton::~NotifierButton() {}

void NotifierSettingsView::NotifierButton::UpdateIconImage(
    const gfx::Image& icon) {
  notifier_->icon = icon;
  if (icon.IsEmpty()) {
    icon_view_->SetImage(gfx::CreateVectorIcon(
        message_center::kProductIcon, kEntryIconSize, gfx::kChromeIconGrey));
  } else {
    icon_view_->SetImage(icon.ToImageSkia());
    icon_view_->SetImageSize(gfx::Size(kEntryIconSize, kEntryIconSize));
  }
  GridChanged(ShouldHaveLearnMoreButton());
}

void NotifierSettingsView::NotifierButton::SetChecked(bool checked) {
  checkbox_->SetChecked(checked);
  notifier_->enabled = checked;
}

bool NotifierSettingsView::NotifierButton::checked() const {
  return checkbox_->checked();
}

bool NotifierSettingsView::NotifierButton::has_learn_more() const {
  return learn_more_ != nullptr;
}

const Notifier& NotifierSettingsView::NotifierButton::notifier() const {
  return *notifier_.get();
}

void NotifierSettingsView::NotifierButton::SendLearnMorePressedForTest() {
  if (learn_more_ == nullptr)
    return;
  gfx::Point point(110, 120);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  ButtonPressed(learn_more_, pressed);
}

void NotifierSettingsView::NotifierButton::ButtonPressed(
    views::Button* button,
    const ui::Event& event) {
  if (button == checkbox_) {
    // The checkbox state has already changed at this point, but we'll update
    // the state on NotifierSettingsView::ButtonPressed() too, so here change
    // back to the previous state.
    checkbox_->SetChecked(!checkbox_->checked());
    Button::NotifyClick(event);
  } else if (button == learn_more_) {
    DCHECK(provider_);
    provider_->OnNotifierAdvancedSettingsRequested(notifier_->notifier_id,
                                                   nullptr);
  }
}

void NotifierSettingsView::NotifierButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  static_cast<views::View*>(checkbox_)->GetAccessibleNodeData(node_data);
}

bool NotifierSettingsView::NotifierButton::ShouldHaveLearnMoreButton() const {
  if (!provider_)
    return false;

  return provider_->NotifierHasAdvancedSettings(notifier_->notifier_id);
}

void NotifierSettingsView::NotifierButton::GridChanged(bool has_learn_more) {
  using views::ColumnSet;
  using views::GridLayout;

  GridLayout* layout = GridLayout::CreateAndInstall(this);
  ColumnSet* cs = layout->AddColumnSet(0);
  // Add a column for the checkbox.
  cs->AddPaddingColumn(0, kInnateCheckboxRightPadding);
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::FIXED,
                kComputedCheckboxSize, 0);
  cs->AddPaddingColumn(0, kInternalHorizontalSpacing);

  // Add a column for the icon.
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::FIXED,
                kEntryIconSize, 0);
  cs->AddPaddingColumn(0, kSmallerInternalHorizontalSpacing);

  // Add a column for the name.
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  // Add a padding column which contains expandable blank space.
  cs->AddPaddingColumn(1, 0);

  // Add a column for the learn more button if necessary.
  if (has_learn_more) {
    cs->AddPaddingColumn(0, kInternalHorizontalSpacing);
    cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                  GridLayout::USE_PREF, 0, 0);
  }

  layout->StartRow(0, 0);
  layout->AddView(checkbox_);
  layout->AddView(icon_view_);
  layout->AddView(name_view_);
  if (has_learn_more)
    layout->AddView(learn_more_);

  Layout();
}

// NotifierSettingsView -------------------------------------------------------

NotifierSettingsView::NotifierSettingsView(NotifierSettingsProvider* provider)
    : title_arrow_(nullptr),
      header_view_(nullptr),
      scroller_(nullptr),
      provider_(provider) {
  // |provider_| may be null in tests.
  if (provider_)
    provider_->AddObserver(this);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetBackground(
      views::CreateSolidBackground(message_center_style::kBackgroundColor));
  SetPaintToLayer();

  header_view_ = new views::View;
  header_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, kHeaderViewPadding, 0));

  views::View* quiet_mode_view = new views::View;

  views::BoxLayout* quiet_mode_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, kQuietModeViewPadding,
                           kQuietModeViewSpacing);
  quiet_mode_view->SetLayoutManager(quiet_mode_layout);

  views::ImageView* quiet_mode_icon = new views::ImageView();
  quiet_mode_icon->SetImage(gfx::CreateVectorIcon(
      kNotificationCenterDoNotDisturbOffIcon,
      message_center_style::kVectorIconSize, kQuietModeIconColor));
  quiet_mode_view->AddChildView(quiet_mode_icon);

  views::Label* quiet_mode_label = new views::Label(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP));
  quiet_mode_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // "Roboto-Regular, 13sp" is specified in the mock.
  quiet_mode_label->SetFontList(
      message_center_style::GetFontListForSizeAndWeight(
          kLabelFontSize, gfx::Font::Weight::NORMAL));
  quiet_mode_label->SetAutoColorReadabilityEnabled(false);
  quiet_mode_label->SetEnabledColor(kLabelColor);
  quiet_mode_view->AddChildView(quiet_mode_label);
  quiet_mode_layout->SetFlexForView(quiet_mode_label, 1);

  quiet_mode_toggle_ = new views::ToggleButton(this);
  SetQuietModeState(MessageCenter::Get()->IsQuietMode());
  quiet_mode_view->AddChildView(quiet_mode_toggle_);
  header_view_->AddChildView(quiet_mode_view);

  views::Label* top_label = new views::Label(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION));
  top_label->SetBorder(views::CreateEmptyBorder(kTopLabelPadding));
  // "Roboto-Medium, 13sp" is specified in the mock.
  top_label->SetFontList(message_center_style::GetFontListForSizeAndWeight(
      kLabelFontSize, gfx::Font::Weight::MEDIUM));
  top_label->SetAutoColorReadabilityEnabled(false);
  top_label->SetEnabledColor(kTopLabelColor);
  top_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  top_label->SetMultiLine(true);
  header_view_->AddChildView(top_label);

  AddChildView(header_view_);

  scroller_ = new views::ScrollView();
  scroller_->SetBackgroundColor(message_center_style::kBackgroundColor);
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  scroller_->SetHorizontalScrollBar(new views::OverlayScrollBar(true));
  AddChildView(scroller_);

  std::vector<std::unique_ptr<Notifier>> notifiers;
  if (provider_)
    provider_->GetNotifierList(&notifiers);

  UpdateContentsView(std::move(notifiers));
}

NotifierSettingsView::~NotifierSettingsView() {
  // |provider_| may be null in tests.
  if (provider_)
    provider_->RemoveObserver(this);
}

bool NotifierSettingsView::IsScrollable() {
  return scroller_->height() < scroller_->contents()->height();
}

void NotifierSettingsView::SetQuietModeState(bool is_quiet_mode) {
  quiet_mode_toggle_->SetIsOn(is_quiet_mode, false /* animate */);
}

void NotifierSettingsView::UpdateIconImage(const NotifierId& notifier_id,
                                           const gfx::Image& icon) {
  for (std::set<NotifierButton*>::iterator iter = buttons_.begin();
       iter != buttons_.end(); ++iter) {
    if ((*iter)->notifier().notifier_id == notifier_id) {
      (*iter)->UpdateIconImage(icon);
      return;
    }
  }
}

void NotifierSettingsView::NotifierGroupChanged() {
  std::vector<std::unique_ptr<Notifier>> notifiers;
  if (provider_)
    provider_->GetNotifierList(&notifiers);

  UpdateContentsView(std::move(notifiers));
}

void NotifierSettingsView::NotifierEnabledChanged(const NotifierId& notifier_id,
                                                  bool enabled) {}

void NotifierSettingsView::UpdateContentsView(
    std::vector<std::unique_ptr<Notifier>> notifiers) {
  buttons_.clear();

  views::View* contents_view = new views::View();
  contents_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(0, kHorizontalMargin)));

  size_t notifier_count = notifiers.size();
  for (size_t i = 0; i < notifier_count; ++i) {
    NotifierButton* button =
        new NotifierButton(provider_, std::move(notifiers[i]), this);
    EntryView* entry = new EntryView(button);

    entry->SetFocusBehavior(FocusBehavior::ALWAYS);
    contents_view->AddChildView(entry);
    buttons_.insert(button);
  }

  scroller_->SetContents(contents_view);

  contents_view->SetBoundsRect(gfx::Rect(contents_view->GetPreferredSize()));
  InvalidateLayout();
}

void NotifierSettingsView::Layout() {
  int header_height = header_view_->GetHeightForWidth(width());
  header_view_->SetBounds(0, 0, width(), header_height);

  views::View* contents_view = scroller_->contents();
  int content_width = width();
  int content_height = contents_view->GetHeightForWidth(content_width);
  if (header_height + content_height > height()) {
    content_width -= scroller_->GetScrollBarLayoutWidth();
    content_height = contents_view->GetHeightForWidth(content_width);
  }
  contents_view->SetBounds(0, 0, content_width, content_height);
  scroller_->SetBounds(0, header_height, width(), height() - header_height);
}

gfx::Size NotifierSettingsView::GetMinimumSize() const {
  gfx::Size size(kWidth, kMinimumHeight);
  int total_height = header_view_->GetPreferredSize().height() +
                     scroller_->contents()->GetPreferredSize().height();
  if (total_height > kMinimumHeight)
    size.Enlarge(scroller_->GetScrollBarLayoutWidth(), 0);
  return size;
}

gfx::Size NotifierSettingsView::CalculatePreferredSize() const {
  gfx::Size preferred_size;
  gfx::Size header_size = header_view_->GetPreferredSize();
  gfx::Size content_size = scroller_->contents()->GetPreferredSize();
  return gfx::Size(std::max(header_size.width(), content_size.width()),
                   header_size.height() + content_size.height());
}

bool NotifierSettingsView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    GetWidget()->Close();
    return true;
  }

  return scroller_->OnKeyPressed(event);
}

bool NotifierSettingsView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return scroller_->OnMouseWheel(event);
}

void NotifierSettingsView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  if (sender == title_arrow_) {
    MessageCenterView* center_view = static_cast<MessageCenterView*>(parent());
    center_view->SetSettingsVisible(!center_view->settings_visible());
    return;
  }

  if (sender == quiet_mode_toggle_) {
    MessageCenter::Get()->SetQuietMode(quiet_mode_toggle_->is_on());
    return;
  }

  std::set<NotifierButton*>::iterator iter =
      buttons_.find(static_cast<NotifierButton*>(sender));

  if (iter == buttons_.end())
    return;

  (*iter)->SetChecked(!(*iter)->checked());
  if (provider_)
    provider_->SetNotifierEnabled((*iter)->notifier().notifier_id,
                                  (*iter)->checked());
}

}  // namespace ash
