// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include <utility>

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager_delegate.h"
#include "components/autofill/browser/wallet/wallet_service_url.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

namespace {

// The minimum useful height of the contents area of the dialog.
const int kMinimumContentsHeight = 100;

// Horizontal padding between text and other elements (in pixels).
const int kAroundTextPadding = 4;

// Size of the triangular mark that indicates an invalid textfield (in pixels).
const size_t kDogEarSize = 10;

// The space between the edges of a notification bar and the text within (in
// pixels).
const size_t kNotificationPadding = 14;

// Vertical padding above and below each detail section (in pixels).
const size_t kDetailSectionInset = 10;

const size_t kAutocheckoutProgressBarWidth = 300;
const size_t kAutocheckoutProgressBarHeight = 11;

const size_t kArrowHeight = 7;
const size_t kArrowWidth = 2 * kArrowHeight;

// The padding around the edges of the legal documents text, in pixels.
const size_t kLegalDocPadding = 20;

// Slight shading for mouse hover and legal document background.
SkColor kShadingColor = SkColorSetARGB(7, 0, 0, 0);

// A border color for the legal document view.
SkColor kSubtleBorderColor = SkColorSetARGB(10, 0, 0, 0);

// The top padding, in pixels, for the suggestions menu dropdown arrows.
const size_t kMenuButtonTopOffset = 5;

// The side padding, in pixels, for the suggestions menu dropdown arrows.
const size_t kMenuButtonHorizontalPadding = 20;

const char kDecoratedTextfieldClassName[] = "autofill/DecoratedTextfield";
const char kNotificationAreaClassName[] = "autofill/NotificationArea";

views::Border* CreateLabelAlignmentBorder() {
  // TODO(estade): this should be made to match the native textfield top
  // inset. It's hard to get at, so for now it's hard-coded.
  return views::Border::CreateEmptyBorder(4, 0, 0, 0);
}

// Returns a label that describes a details section.
views::Label* CreateDetailsSectionLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  label->SetFont(label->font().DeriveFont(0, gfx::Font::BOLD));
  label->set_border(CreateLabelAlignmentBorder());
  return label;
}

// Draws an arrow at the top of |canvas| pointing to |tip_x|.
void DrawArrow(gfx::Canvas* canvas, int tip_x, const SkColor& color) {
  const int arrow_half_width = kArrowWidth / 2.0f;
  const int arrow_middle = tip_x - arrow_half_width;

  SkPath arrow;
  arrow.moveTo(arrow_middle - arrow_half_width, kArrowHeight);
  arrow.lineTo(arrow_middle + arrow_half_width, kArrowHeight);
  arrow.lineTo(arrow_middle, 0);
  arrow.close();
  canvas->ClipPath(arrow);
  canvas->DrawColor(color);
}

// This class handles layout for the first row of a SuggestionView.
// It exists to circumvent shortcomings of GridLayout and BoxLayout (namely that
// the former doesn't fully respect child visibility, and that the latter won't
// expand a single child).
class SectionRowView : public views::View {
 public:
  SectionRowView() {}
  virtual ~SectionRowView() {}

  // views::View implementation:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    // Only the height matters anyway.
    return child_at(2)->GetPreferredSize();
  }

  virtual void Layout() OVERRIDE {
    const gfx::Rect bounds = GetContentsBounds();

    // Icon is left aligned.
    int start_x = bounds.x();
    views::View* icon = child_at(0);
    if (icon->visible()) {
      icon->SizeToPreferredSize();
      icon->SetX(start_x);
      icon->SetY(bounds.y() +
          (bounds.height() - icon->bounds().height()) / 2);
      start_x += icon->bounds().width() + kAroundTextPadding;
    }

    // Textfield is right aligned.
    int end_x = bounds.width();
    views::View* decorated = child_at(2);
    if (decorated->visible()) {
      decorated->SizeToPreferredSize();
      decorated->SetX(bounds.width() - decorated->bounds().width());
      decorated->SetY(bounds.y());
      end_x = decorated->bounds().x() - kAroundTextPadding;
    }

    // Label takes up all the space in between.
    views::View* label = child_at(1);
    if (label->visible())
      label->SetBounds(start_x, bounds.y(), end_x - start_x, bounds.height());

    views::View::Layout();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SectionRowView);
};

// This view is used for the contents of the error bubble widget.
class ErrorBubbleContents : public views::View {
 public:
  explicit ErrorBubbleContents(const string16& message)
      : color_(SK_ColorWHITE) {
    DialogNotification notification(DialogNotification::VALIDATION_ERROR,
                                    string16());
    color_ = notification.GetBackgroundColor();

    set_border(views::Border::CreateEmptyBorder(kArrowHeight, 0, 0, 0));

    views::Label* label = new views::Label();
    label->SetText(message);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(SK_ColorWHITE);
    label->set_border(
        views::Border::CreateSolidSidedBorder(5, 10, 5, 10, color_));
    label->set_background(
        views::Background::CreateSolidBackground(color_));
    SetLayoutManager(new views::FillLayout());
    AddChildView(label);
  }
  virtual ~ErrorBubbleContents() {}

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    views::View::OnPaint(canvas);
    DrawArrow(canvas, width() / 2.0f, color_);
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(ErrorBubbleContents);
};

}  // namespace

// AutofillDialogViews::SizeLimitedScrollView ----------------------------------

AutofillDialogViews::SizeLimitedScrollView::SizeLimitedScrollView(
    views::View* scroll_contents)
    : max_height_(-1) {
  set_hide_horizontal_scrollbar(true);
  SetContents(scroll_contents);
}

AutofillDialogViews::SizeLimitedScrollView::~SizeLimitedScrollView() {}

void AutofillDialogViews::SizeLimitedScrollView::Layout() {
  contents()->SizeToPreferredSize();
  ScrollView::Layout();
}

gfx::Size AutofillDialogViews::SizeLimitedScrollView::GetPreferredSize() {
  gfx::Size size = contents()->GetPreferredSize();
  if (max_height_ >= 0 && max_height_ < size.height())
    size.set_height(max_height_);

  return size;
}

void AutofillDialogViews::SizeLimitedScrollView::SetMaximumHeight(
    int max_height) {
  int old_max = max_height_;
  max_height_ = max_height;

  if (max_height_ < height() || old_max <= height())
    PreferredSizeChanged();
}

// AutofillDialogViews::ErrorBubble --------------------------------------------

AutofillDialogViews::ErrorBubble::ErrorBubble(views::View* anchor,
                                              const string16& message)
    : anchor_(anchor),
      observer_(this) {
  ErrorBubbleContents* contents = new ErrorBubbleContents(message);

  widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  views::Widget* anchor_widget = anchor->GetWidget();
  params.parent = anchor_widget->GetNativeView();

  gfx::Rect anchor_bounds = anchor->GetBoundsInScreen();
  gfx::Rect bubble_bounds;
  bubble_bounds.set_size(contents->GetPreferredSize());
  bubble_bounds.set_x(anchor_bounds.right() -
      (anchor_bounds.width() + bubble_bounds.width()) / 2);
  const int kErrorBubbleOverlap = 3;
  bubble_bounds.set_y(anchor_bounds.bottom() - kErrorBubbleOverlap);
  params.bounds = bubble_bounds;

  widget_->Init(params);
  widget_->SetContentsView(contents);
  widget_->Show();
  observer_.Add(widget_);
}

AutofillDialogViews::ErrorBubble::~ErrorBubble() {
  if (widget_)
    widget_->Hide();
}

bool AutofillDialogViews::ErrorBubble::IsShowing() {
  return widget_ && widget_->IsVisible();
}

void AutofillDialogViews::ErrorBubble::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget_, widget);
  observer_.Remove(widget_);
  widget_ = NULL;
}

// AutofillDialogViews::DecoratedTextfield -------------------------------------

AutofillDialogViews::DecoratedTextfield::DecoratedTextfield(
    const string16& default_value,
    const string16& placeholder,
    views::TextfieldController* controller)
    : textfield_(new views::Textfield()),
      invalid_(false) {
  textfield_->set_placeholder_text(placeholder);
  textfield_->SetText(default_value);
  textfield_->SetController(controller);
  SetLayoutManager(new views::FillLayout());
  AddChildView(textfield_);
}

AutofillDialogViews::DecoratedTextfield::~DecoratedTextfield() {}

void AutofillDialogViews::DecoratedTextfield::SetInvalid(bool invalid) {
  invalid_ = invalid;
  // TODO(estade): Red is not exactly the right color.
  if (invalid)
    textfield_->SetBorderColor(SK_ColorRED);
  else
    textfield_->UseDefaultBorderColor();
  SchedulePaint();
}

std::string AutofillDialogViews::DecoratedTextfield::GetClassName() const {
  return kDecoratedTextfieldClassName;
}

void AutofillDialogViews::DecoratedTextfield::PaintChildren(
    gfx::Canvas* canvas) {}

void AutofillDialogViews::DecoratedTextfield::OnPaint(gfx::Canvas* canvas) {
  // Draw the textfield first.
  canvas->Save();
  if (FlipCanvasOnPaintForRTLUI()) {
    canvas->Translate(gfx::Vector2d(width(), 0));
    canvas->Scale(-1, 1);
  }
  views::View::PaintChildren(canvas);
  canvas->Restore();

  // Then draw extra stuff on top.
  if (invalid_) {
    SkPath dog_ear;
    dog_ear.moveTo(width() - kDogEarSize, 0);
    dog_ear.lineTo(width(), 0);
    dog_ear.lineTo(width(), kDogEarSize);
    dog_ear.close();
    canvas->ClipPath(dog_ear);
    // TODO(estade): Red is not exactly the right color.
    canvas->DrawColor(SK_ColorRED);
  }
}

// AutofillDialogViews::AccountChooser -----------------------------------------

AutofillDialogViews::AccountChooser::AccountChooser(
    AutofillDialogController* controller)
    : image_(new views::ImageView()),
      label_(new views::Label()),
      arrow_(new views::ImageView()),
      link_(new views::Link(controller->SignInLinkText())),
      controller_(controller) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                           kAroundTextPadding));
  AddChildView(image_);
  AddChildView(label_);

  arrow_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_MENU_DROPARROW).ToImageSkia());
  AddChildView(arrow_);

  link_->set_listener(this);
  AddChildView(link_);
}

AutofillDialogViews::AccountChooser::~AccountChooser() {}

void AutofillDialogViews::AccountChooser::Update() {
  image_->SetImage(controller_->AccountChooserImage().AsImageSkia());
  label_->SetText(controller_->AccountChooserText());

  bool show_link = !controller_->MenuModelForAccountChooser();
  label_->SetVisible(!show_link);
  arrow_->SetVisible(!show_link);
  link_->SetVisible(show_link);

  menu_runner_.reset();

  PreferredSizeChanged();
}

void AutofillDialogViews::AccountChooser::SetSignInLinkEnabled(bool enabled) {
  link_->SetEnabled(enabled);
}

bool AutofillDialogViews::AccountChooser::OnMousePressed(
    const ui::MouseEvent& event) {
  // Return true so we get the release event.
  if (controller_->MenuModelForAccountChooser())
    return event.IsOnlyLeftMouseButton();

  return false;
}

void AutofillDialogViews::AccountChooser::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location()))
    return;

  ui::MenuModel* model = controller_->MenuModelForAccountChooser();
  if (!model)
    return;

  menu_runner_.reset(new views::MenuRunner(model));
  ignore_result(
      menu_runner_->RunMenuAt(GetWidget(),
                              NULL,
                              GetBoundsInScreen(),
                              views::MenuItemView::TOPRIGHT,
                              0));
}

void AutofillDialogViews::AccountChooser::LinkClicked(views::Link* source,
                                                      int event_flags) {
  controller_->StartSignInFlow();
}

// AutofillDialogViews::NotificationArea ---------------------------------------

AutofillDialogViews::NotificationArea::NotificationArea(
    AutofillDialogController* controller)
    : controller_(controller),
      checkbox_(NULL) {
  // Reserve vertical space for the arrow (regardless of whether one exists).
  set_border(views::Border::CreateEmptyBorder(kArrowHeight, 0, 0, 0));

  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  SetLayoutManager(box_layout);
}

AutofillDialogViews::NotificationArea::~NotificationArea() {}

void AutofillDialogViews::NotificationArea::SetNotifications(
    const std::vector<DialogNotification>& notifications) {
  notifications_ = notifications;

  RemoveAllChildViews(true);
  checkbox_ = NULL;

  if (notifications_.empty())
    return;

  for (size_t i = 0; i < notifications_.size(); ++i) {
    const DialogNotification& notification = notifications_[i];

    scoped_ptr<views::View> view;
    if (notification.HasCheckbox()) {
      scoped_ptr<views::Checkbox> checkbox(new views::Checkbox(string16()));
      checkbox_ = checkbox.get();
      // We have to do this instead of using set_border() because a border
      // is being used to draw the check square.
      static_cast<views::CheckboxNativeThemeBorder*>(checkbox->border())->
          SetCustomInsets(gfx::Insets(kNotificationPadding,
                                      kNotificationPadding,
                                      kNotificationPadding,
                                      kNotificationPadding));
      if (!notification.interactive())
        checkbox->SetState(views::Button::STATE_DISABLED);
      checkbox->SetText(notification.display_text());
      checkbox->SetMultiLine(true);
      checkbox->set_alignment(views::TextButtonBase::ALIGN_LEFT);
      checkbox->SetEnabledColor(notification.GetTextColor());
      checkbox->SetHoverColor(notification.GetTextColor());
      checkbox->SetChecked(notification.checked());
      checkbox->set_listener(this);
      view.reset(checkbox.release());
    } else {
      scoped_ptr<views::Label> label(new views::Label());
      label->SetText(notification.display_text());
      label->SetMultiLine(true);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      label->SetAutoColorReadabilityEnabled(false);
      label->SetEnabledColor(notification.GetTextColor());
      label->set_border(views::Border::CreateSolidBorder(
          kNotificationPadding, notification.GetBackgroundColor()));
      view.reset(label.release());
    }

    view->set_background(views::Background::CreateSolidBackground(
        notification.GetBackgroundColor()));
    AddChildView(view.release());
  }

  PreferredSizeChanged();
}

std::string AutofillDialogViews::NotificationArea::GetClassName() const {
  return kNotificationAreaClassName;
}

void AutofillDialogViews::NotificationArea::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  if (HasArrow()) {
    DrawArrow(
        canvas,
        GetMirroredXInView(width() - arrow_centering_anchor_->width() / 2.0f),
        notifications_[0].GetBackgroundColor());
  }
}

void AutofillDialogViews::OnWidgetClosing(views::Widget* widget) {
  observer_.Remove(widget);
}

void AutofillDialogViews::OnWidgetBoundsChanged(views::Widget* widget,
                                                const gfx::Rect& new_bounds) {
  int non_scrollable_height = window_->GetContentsView()->bounds().height() -
      scrollable_area_->bounds().height();
  int browser_window_height = widget->GetContentsView()->bounds().height();

  scrollable_area_->SetMaximumHeight(
      std::max(kMinimumContentsHeight,
               (browser_window_height - non_scrollable_height) * 8 / 10));
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::NotificationArea::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(sender, checkbox_);
  controller_->NotificationCheckboxStateChanged(notifications_.front().type(),
                                                checkbox_->checked());
}

bool AutofillDialogViews::NotificationArea::HasArrow() {
  return !notifications_.empty() && notifications_[0].HasArrow() &&
      arrow_centering_anchor_.get();
}

// AutofillDialogViews::SectionContainer ---------------------------------------

AutofillDialogViews::SectionContainer::SectionContainer(
    const string16& label,
    views::View* controls,
    views::Button* proxy_button)
    : proxy_button_(proxy_button),
      forward_mouse_events_(false) {
  set_notify_enter_exit_on_child(true);

  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(kDetailSectionInset, 0, kDetailSectionInset, 0);
  SetLayoutManager(layout);

  const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  // TODO(estade): pull out these constants, and figure out better values
  // for them.
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        180,
                        0);
  column_set->AddPaddingColumn(0, 30);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        300,
                        0);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(CreateDetailsSectionLabel(label));
  layout->AddView(controls);
}

AutofillDialogViews::SectionContainer::~SectionContainer() {}

void AutofillDialogViews::SectionContainer::SetActive(bool active) {
  bool is_active = active && proxy_button_->visible();
  if (is_active == !!background())
    return;

  set_background(is_active ?
      views::Background::CreateSolidBackground(kShadingColor) :
      NULL);
  SchedulePaint();
}

void AutofillDialogViews::SectionContainer::SetForwardMouseEvents(
    bool forward) {
  forward_mouse_events_ = forward;
  if (!forward)
    set_background(NULL);
}

void AutofillDialogViews::SectionContainer::OnMouseMoved(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  SetActive(true);
}

void AutofillDialogViews::SectionContainer::OnMouseEntered(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  SetActive(true);
  proxy_button_->OnMouseEntered(ProxyEvent(event));
  SchedulePaint();
}

void AutofillDialogViews::SectionContainer::OnMouseExited(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  SetActive(false);
  proxy_button_->OnMouseExited(ProxyEvent(event));
  SchedulePaint();
}

bool AutofillDialogViews::SectionContainer::OnMousePressed(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return false;

  return proxy_button_->OnMousePressed(ProxyEvent(event));
}

void AutofillDialogViews::SectionContainer::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  proxy_button_->OnMouseReleased(ProxyEvent(event));
}

// static
ui::MouseEvent AutofillDialogViews::SectionContainer::ProxyEvent(
    const ui::MouseEvent& event) {
  ui::MouseEvent event_copy = event;
  event_copy.set_location(gfx::Point());
  return event_copy;
}

// AutofilDialogViews::SuggestionView ------------------------------------------

AutofillDialogViews::SuggestionView::SuggestionView(
    const string16& edit_label,
    AutofillDialogViews* autofill_dialog)
    : label_(new views::Label()),
      label_line_2_(new views::Label()),
      icon_(new views::ImageView()),
      label_container_(new SectionRowView()),
      decorated_(
          new DecoratedTextfield(string16(), string16(), autofill_dialog)),
      edit_link_(new views::Link(edit_label)) {
  // Label and icon.
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->set_border(CreateLabelAlignmentBorder());
  label_container_->AddChildView(icon_);
  label_container_->AddChildView(label_);
  label_container_->AddChildView(decorated_);
  decorated_->SetVisible(false);
  // TODO(estade): get the sizing and spacing right on this textfield.
  decorated_->textfield()->set_default_width_in_chars(10);
  AddChildView(label_container_);

  label_line_2_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_line_2_->SetVisible(false);
  AddChildView(label_line_2_);

  // TODO(estade): The link needs to have a different color when hovered.
  edit_link_->set_listener(autofill_dialog);
  edit_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  edit_link_->SetUnderline(false);

  // This container prevents the edit link from being horizontally stretched.
  views::View* link_container = new views::View();
  link_container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  link_container->AddChildView(edit_link_);
  AddChildView(link_container);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
}

AutofillDialogViews::SuggestionView::~SuggestionView() {}

void AutofillDialogViews::SuggestionView::SetEditable(bool editable) {
  edit_link_->SetVisible(editable);
}

void AutofillDialogViews::SuggestionView::SetSuggestionText(
    const string16& text,
    gfx::Font::FontStyle text_style) {
  label_->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont).DeriveFont(0, text_style));

  // TODO(estade): does this localize well?
  string16 line_return(ASCIIToUTF16("\n"));
  size_t position = text.find(line_return);
  if (position == string16::npos) {
    label_->SetText(text);
    label_line_2_->SetVisible(false);
  } else {
    label_->SetText(text.substr(0, position));
    label_line_2_->SetText(text.substr(position + line_return.length()));
    label_line_2_->SetVisible(true);
  }
}

void AutofillDialogViews::SuggestionView::SetSuggestionIcon(
    const gfx::Image& image) {
  icon_->SetVisible(!image.IsEmpty());
  icon_->SetImage(image.AsImageSkia());
}

void AutofillDialogViews::SuggestionView::ShowTextfield(
    const string16& placeholder_text,
    const gfx::ImageSkia& icon) {
  decorated_->textfield()->set_placeholder_text(placeholder_text);
  decorated_->textfield()->SetIcon(icon);
  decorated_->SetVisible(true);
  // The textfield will increase the height of the first row and cause the
  // label to be aligned properly, so the border is not necessary.
  label_->set_border(NULL);
}

// AutofilDialogViews::AutocheckoutProgressBar ---------------------------------

AutofillDialogViews::AutocheckoutProgressBar::AutocheckoutProgressBar() {}

gfx::Size AutofillDialogViews::AutocheckoutProgressBar::GetPreferredSize() {
  return gfx::Size(kAutocheckoutProgressBarWidth,
                   kAutocheckoutProgressBarHeight);
}

// AutofillDialogView ----------------------------------------------------------

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViews(controller);
}

// AutofillDialogViews ---------------------------------------------------------

AutofillDialogViews::AutofillDialogViews(AutofillDialogController* controller)
    : controller_(controller),
      window_(NULL),
      contents_(NULL),
      notification_area_(NULL),
      account_chooser_(NULL),
      sign_in_container_(NULL),
      cancel_sign_in_(NULL),
      sign_in_webview_(NULL),
      main_container_(NULL),
      scrollable_area_(NULL),
      details_container_(NULL),
      button_strip_extra_view_(NULL),
      save_in_chrome_checkbox_(NULL),
      autocheckout_progress_bar_view_(NULL),
      autocheckout_progress_bar_(NULL),
      footnote_view_(NULL),
      legal_document_view_(NULL),
      focus_manager_(NULL),
      observer_(this) {
  DCHECK(controller);
  detail_groups_.insert(std::make_pair(SECTION_EMAIL,
                                       DetailsGroup(SECTION_EMAIL)));
  detail_groups_.insert(std::make_pair(SECTION_CC,
                                       DetailsGroup(SECTION_CC)));
  detail_groups_.insert(std::make_pair(SECTION_BILLING,
                                       DetailsGroup(SECTION_BILLING)));
  detail_groups_.insert(std::make_pair(SECTION_CC_BILLING,
                                       DetailsGroup(SECTION_CC_BILLING)));
  detail_groups_.insert(std::make_pair(SECTION_SHIPPING,
                                       DetailsGroup(SECTION_SHIPPING)));
}

AutofillDialogViews::~AutofillDialogViews() {
  DCHECK(!window_);
}

void AutofillDialogViews::Show() {
  InitChildViews();
  UpdateAccountChooser();
  UpdateNotificationArea();

  // Ownership of |contents_| is handed off by this call. The widget will take
  // care of deleting itself after calling DeleteDelegate().
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(
          controller_->web_contents());
  window_ = CreateWebContentsModalDialogViews(
      this,
      controller_->web_contents()->GetView()->GetNativeView(),
      web_contents_modal_dialog_manager->delegate()->
          GetWebContentsModalDialogHost());
  web_contents_modal_dialog_manager->ShowDialog(window_->GetNativeView());
  focus_manager_ = window_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);

  // Listen for size changes on the browser.
  views::Widget* browser_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller_->web_contents()->GetView()->GetNativeView());
  observer_.Add(browser_widget);
  OnWidgetBoundsChanged(browser_widget, gfx::Rect());
}

void AutofillDialogViews::Hide() {
  if (window_)
    window_->Close();
}

void AutofillDialogViews::UpdateAccountChooser() {
  account_chooser_->Update();

  // Update legal documents for the account.
  if (footnote_view_) {
    string16 text = controller_->LegalDocumentsText();
    if (text.empty()) {
      footnote_view_->SetVisible(false);
    } else {
      footnote_view_->SetVisible(true);
      legal_document_view_->SetText(text);

      const std::vector<ui::Range>& link_ranges =
          controller_->LegalDocumentLinks();
      for (size_t i = 0; i < link_ranges.size(); ++i) {
        legal_document_view_->AddStyleRange(
            link_ranges[i],
            views::StyledLabel::RangeStyleInfo::CreateForLink());
      }
    }

    ContentsPreferredSizeChanged();
  }
}

void AutofillDialogViews::UpdateButtonStrip() {
  button_strip_extra_view_->SetVisible(
      GetDialogButtons() != ui::DIALOG_BUTTON_NONE);
  save_in_chrome_checkbox_->SetVisible(!(controller_->AutocheckoutIsRunning() ||
                                         controller_->HadAutocheckoutError()));
  autocheckout_progress_bar_view_->SetVisible(
      controller_->AutocheckoutIsRunning());
  details_container_->SetVisible(!(controller_->AutocheckoutIsRunning() ||
                                   controller_->HadAutocheckoutError()));

  GetDialogClientView()->UpdateDialogButtons();
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateNotificationArea() {
  DCHECK(notification_area_);
  notification_area_->SetNotifications(controller_->CurrentNotifications());
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateSection(DialogSection section) {
  UpdateSectionImpl(section, true);
}

void AutofillDialogViews::FillSection(DialogSection section,
                                      const DetailInput& originating_input) {
  DetailsGroup* group = GroupForSection(section);
  TextfieldMap::iterator text_mapping =
      group->textfields.find(&originating_input);
  if (text_mapping != group->textfields.end())
    text_mapping->second->textfield()->SetText(string16());

  UpdateSectionImpl(section, false);
}

void AutofillDialogViews::GetUserInput(DialogSection section,
                                       DetailOutputMap* output) {
  DetailsGroup* group = GroupForSection(section);
  for (TextfieldMap::const_iterator it = group->textfields.begin();
       it != group->textfields.end(); ++it) {
    output->insert(std::make_pair(it->first, it->second->textfield()->text()));
  }
  for (ComboboxMap::const_iterator it = group->comboboxes.begin();
       it != group->comboboxes.end(); ++it) {
    output->insert(std::make_pair(it->first,
        it->second->model()->GetItemAt(it->second->selected_index())));
  }
}

string16 AutofillDialogViews::GetCvc() {
  DialogSection billing_section = controller_->SectionIsActive(SECTION_CC) ?
      SECTION_CC : SECTION_CC_BILLING;
  return GroupForSection(billing_section)->suggested_info->
      decorated_textfield()->textfield()->text();
}

bool AutofillDialogViews::SaveDetailsLocally() {
  return save_in_chrome_checkbox_->checked();
}

const content::NavigationController* AutofillDialogViews::ShowSignIn() {
  // TODO(abodenha) We should be able to use the WebContents of the WebView
  // to navigate instead of LoadInitialURL.  Figure out why it doesn't work.

  account_chooser_->SetSignInLinkEnabled(false);
  sign_in_webview_->LoadInitialURL(wallet::GetSignInUrl());
  // TODO(abodenha) Resize the dialog to avoid the need for a scroll bar on
  // sign in. See http://crbug.com/169286
  sign_in_webview_->SetPreferredSize(contents_->GetPreferredSize());
  main_container_->SetVisible(false);
  sign_in_container_->SetVisible(true);
  UpdateButtonStrip();
  ContentsPreferredSizeChanged();
  return &sign_in_webview_->web_contents()->GetController();
}

void AutofillDialogViews::HideSignIn() {
  account_chooser_->SetSignInLinkEnabled(true);
  sign_in_container_->SetVisible(false);
  main_container_->SetVisible(true);
  UpdateButtonStrip();
  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateProgressBar(double value) {
  autocheckout_progress_bar_->SetValue(value);
}

void AutofillDialogViews::ModelChanged() {
  menu_runner_.reset();

  for (DetailGroupMap::const_iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    UpdateDetailsGroupState(iter->second);
  }
}

TestableAutofillDialogView* AutofillDialogViews::GetTestableView() {
  return this;
}

void AutofillDialogViews::SubmitForTesting() {
  Accept();
}

void AutofillDialogViews::CancelForTesting() {
  if (Cancel())
    Hide();
}

string16 AutofillDialogViews::GetTextContentsOfInput(const DetailInput& input) {
  views::Textfield* textfield = TextfieldForInput(input);
  if (textfield)
    return textfield->text();

  views::Combobox* combobox = ComboboxForInput(input);
  if (combobox)
    return combobox->model()->GetItemAt(combobox->selected_index());

  NOTREACHED();
  return string16();
}

void AutofillDialogViews::SetTextContentsOfInput(const DetailInput& input,
                                                 const string16& contents) {
  views::Textfield* textfield = TextfieldForInput(input);
  if (textfield) {
    TextfieldForInput(input)->SetText(contents);
    return;
  }

  views::Combobox* combobox = ComboboxForInput(input);
  if (combobox) {
    for (int i = 0; i < combobox->model()->GetItemCount(); ++i) {
      if (contents == combobox->model()->GetItemAt(i)) {
        combobox->SetSelectedIndex(i);
        return;
      }
    }
    // If we don't find a match, return the combobox to its default state.
    combobox->SetSelectedIndex(combobox->model()->GetDefaultIndex());
    return;
  }

  NOTREACHED();
}

void AutofillDialogViews::ActivateInput(const DetailInput& input) {
  TextfieldEditedOrActivated(TextfieldForInput(input), false);
}

string16 AutofillDialogViews::GetWindowTitle() const {
  return controller_->DialogTitle();
}

void AutofillDialogViews::WindowClosing() {
  focus_manager_->RemoveFocusChangeListener(this);
}

void AutofillDialogViews::DeleteDelegate() {
  window_ = NULL;
  // |this| belongs to |controller_|.
  controller_->ViewClosed();
}

views::Widget* AutofillDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* AutofillDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* AutofillDialogViews::GetContentsView() {
  return contents_;
}

int AutofillDialogViews::GetDialogButtons() const {
  if (sign_in_container_->visible())
    return ui::DIALOG_BUTTON_NONE;

  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

string16 AutofillDialogViews::GetDialogButtonLabel(ui::DialogButton button)
    const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonText() : controller_->CancelButtonText();
}

bool AutofillDialogViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  return controller_->IsDialogButtonEnabled(button);
}

views::View* AutofillDialogViews::CreateExtraView() {
  return button_strip_extra_view_;
}

views::View* AutofillDialogViews::CreateTitlebarExtraView() {
  return account_chooser_;
}

views::View* AutofillDialogViews::CreateFootnoteView() {
  footnote_view_ = new views::View();
  footnote_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           kLegalDocPadding,
                           kLegalDocPadding,
                           0));
  footnote_view_->set_border(
      views::Border::CreateSolidSidedBorder(1, 0, 0, 0, kSubtleBorderColor));
  footnote_view_->set_background(
      views::Background::CreateSolidBackground(kShadingColor));

  legal_document_view_ = new views::StyledLabel(string16(), this);
  footnote_view_->AddChildView(legal_document_view_);
  footnote_view_->SetVisible(false);

  return footnote_view_;
}

bool AutofillDialogViews::Cancel() {
  controller_->OnCancel();
  return true;
}

bool AutofillDialogViews::Accept() {
  if (ValidateForm())
    controller_->OnAccept();

  // |controller_| decides when to hide the dialog.
  return false;
}

// TODO(wittman): Remove this override once we move to the new style frame view
// on all dialogs.
views::NonClientFrameView* AutofillDialogViews::CreateNonClientFrameView(
    views::Widget* widget) {
  return CreateConstrainedStyleNonClientFrameView(
      widget,
      controller_->web_contents()->GetBrowserContext());
}

void AutofillDialogViews::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == cancel_sign_in_) {
    controller_->EndSignInFlow();
  } else {
    // TODO(estade): Should the menu be shown on mouse down?
    DetailsGroup* group = NULL;
    for (DetailGroupMap::iterator iter = detail_groups_.begin();
         iter != detail_groups_.end(); ++iter) {
      if (sender == iter->second.suggested_button) {
        group = &iter->second;
        break;
      }
    }
    DCHECK(group);

    if (!group->suggested_button->visible())
      return;

    menu_runner_.reset(new views::MenuRunner(
                           controller_->MenuModelForSection(group->section)));

    group->container->SetActive(true);
    views::Button::ButtonState state = group->suggested_button->state();
    group->suggested_button->SetState(views::Button::STATE_PRESSED);
    // Ignore the result since we don't need to handle a deleted menu specially.
    gfx::Rect bounds = group->suggested_button->GetBoundsInScreen();
    bounds.Inset(group->suggested_button->GetInsets());
    ignore_result(
        menu_runner_->RunMenuAt(sender->GetWidget(),
                                NULL,
                                bounds,
                                views::MenuItemView::TOPRIGHT,
                                0));
    group->container->SetActive(false);
    group->suggested_button->SetState(state);
  }
}

void AutofillDialogViews::ContentsChanged(views::Textfield* sender,
                                          const string16& new_contents) {
  TextfieldEditedOrActivated(sender, true);
}

bool AutofillDialogViews::HandleKeyEvent(views::Textfield* sender,
                                         const ui::KeyEvent& key_event) {
  scoped_ptr<ui::KeyEvent> copy(key_event.Copy());
#if defined(OS_WIN) && !defined(USE_AURA)
  content::NativeWebKeyboardEvent event(copy->native_event());
#else
  content::NativeWebKeyboardEvent event(copy.get());
#endif
  return controller_->HandleKeyPressEventInInput(event);
}

bool AutofillDialogViews::HandleMouseEvent(views::Textfield* sender,
                                           const ui::MouseEvent& mouse_event) {
  if (mouse_event.IsLeftMouseButton() && sender->HasFocus())
    TextfieldEditedOrActivated(sender, false);
  return false;
}

void AutofillDialogViews::OnWillChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  controller_->FocusMoved();
}

void AutofillDialogViews::OnDidChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  if (!focused_before)
    return;

  // If user leaves an edit-field, revalidate the group it belongs to.
  DetailsGroup* group = GroupForView(focused_before);
  if (group && group->container->visible())
    ValidateGroup(*group, AutofillDialogController::VALIDATE_EDIT);
}

void AutofillDialogViews::LinkClicked(views::Link* source, int event_flags) {
  // Edit links.
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    if (iter->second.suggested_info->Contains(source)) {
      controller_->EditClickedForSection(iter->first);
      return;
    }
  }
}

void AutofillDialogViews::OnSelectedIndexChanged(views::Combobox* combobox) {
  DetailsGroup* group = GroupForView(combobox);
  ValidateGroup(*group, AutofillDialogController::VALIDATE_EDIT);
}

void AutofillDialogViews::StyledLabelLinkClicked(const ui::Range& range,
                                                 int event_flags) {
  controller_->LegalDocumentLinkClicked(range);
}

void AutofillDialogViews::InitChildViews() {
  button_strip_extra_view_ = new views::View();
  button_strip_extra_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

  save_in_chrome_checkbox_ =
      new views::Checkbox(controller_->SaveLocallyText());
  button_strip_extra_view_->AddChildView(save_in_chrome_checkbox_);

  autocheckout_progress_bar_view_ = new views::View();
  autocheckout_progress_bar_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  views::Label* progress_bar_label = new views::Label();
  progress_bar_label->SetText(controller_->ProgressBarText());
  progress_bar_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  autocheckout_progress_bar_view_->AddChildView(progress_bar_label);

  autocheckout_progress_bar_ = new AutocheckoutProgressBar();
  autocheckout_progress_bar_view_->AddChildView(autocheckout_progress_bar_);

  button_strip_extra_view_->AddChildView(autocheckout_progress_bar_view_);
  autocheckout_progress_bar_view_->SetVisible(false);

  contents_ = new views::View();
  contents_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  contents_->AddChildView(CreateMainContainer());
  contents_->AddChildView(CreateSignInContainer());
}

views::View* AutofillDialogViews::CreateSignInContainer() {
  sign_in_container_ = new views::View();
  sign_in_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  sign_in_container_->SetVisible(false);
  sign_in_webview_ = new views::WebView(controller_->profile());
  cancel_sign_in_ = new views::LabelButton(this,
                                           controller_->CancelSignInText());
  sign_in_container_->AddChildView(cancel_sign_in_);
  sign_in_container_->AddChildView(sign_in_webview_);
  return sign_in_container_;
}

views::View* AutofillDialogViews::CreateMainContainer() {
  main_container_ = new views::View();
  main_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           views::kRelatedControlVerticalSpacing));

  account_chooser_ = new AccountChooser(controller_);
  if (!views::DialogDelegate::UseNewStyle())
    main_container_->AddChildView(account_chooser_);

  notification_area_ = new NotificationArea(controller_);
  notification_area_->set_arrow_centering_anchor(account_chooser_->AsWeakPtr());
  main_container_->AddChildView(notification_area_);

  scrollable_area_ = new SizeLimitedScrollView(CreateDetailsContainer());
  main_container_->AddChildView(scrollable_area_);
  return main_container_;
}

views::View* AutofillDialogViews::CreateDetailsContainer() {
  details_container_ = new views::View();
  // A box layout is used because it respects widget visibility.
  details_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    CreateDetailsSection(iter->second.section);
    details_container_->AddChildView(iter->second.container);
  }

  return details_container_;
}

void AutofillDialogViews::CreateDetailsSection(DialogSection section) {
  // Inputs container (manual inputs + combobox).
  views::View* inputs_container = CreateInputsContainer(section);

  DetailsGroup* group = GroupForSection(section);
  // Container (holds label + inputs).
  group->container = new SectionContainer(
      controller_->LabelForSection(section),
      inputs_container,
      group->suggested_button);
  UpdateDetailsGroupState(*group);
}

views::View* AutofillDialogViews::CreateInputsContainer(DialogSection section) {
  views::View* inputs_container = new views::View();
  views::GridLayout* layout = new views::GridLayout(inputs_container);
  inputs_container->SetLayoutManager(layout);

  int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  // A column for the menu button.
  column_set->AddColumn(views::GridLayout::CENTER,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  layout->StartRow(0, kColumnSetId);

  // The |info_view| holds |manual_inputs| and |suggested_info|, allowing the
  // dialog to toggle which is shown.
  views::View* info_view = new views::View();
  info_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  views::View* manual_inputs = InitInputsView(section);
  info_view->AddChildView(manual_inputs);
  SuggestionView* suggested_info =
      new SuggestionView(controller_->EditSuggestionText(), this);
  info_view->AddChildView(suggested_info);
  layout->AddView(info_view);

  views::ImageButton* menu_button = new views::ImageButton(this);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  menu_button->SetImage(views::Button::STATE_NORMAL,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON));
  menu_button->SetImage(views::Button::STATE_PRESSED,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_P));
  menu_button->SetImage(views::Button::STATE_HOVERED,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_H));
  menu_button->SetImage(views::Button::STATE_DISABLED,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_D));
  menu_button->set_border(views::Border::CreateEmptyBorder(
      kMenuButtonTopOffset,
      kMenuButtonHorizontalPadding,
      0,
      kMenuButtonHorizontalPadding));
  layout->AddView(menu_button);

  DetailsGroup* group = GroupForSection(section);
  group->suggested_button = menu_button;
  group->manual_input = manual_inputs;
  group->suggested_info = suggested_info;
  return inputs_container;
}

// TODO(estade): we should be using Chrome-style constrained window padding
// values.
views::View* AutofillDialogViews::InitInputsView(DialogSection section) {
  const DetailInputs& inputs = controller_->RequestedFieldsForSection(section);
  TextfieldMap* textfields = &GroupForSection(section)->textfields;
  ComboboxMap* comboboxes = &GroupForSection(section)->comboboxes;

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  for (DetailInputs::const_iterator it = inputs.begin();
       it != inputs.end(); ++it) {
    const DetailInput& input = *it;
    int kColumnSetId = input.row_id;
    views::ColumnSet* column_set = layout->GetColumnSet(kColumnSetId);
    if (!column_set) {
      // Create a new column set and row.
      column_set = layout->AddColumnSet(kColumnSetId);
      if (it != inputs.begin())
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kColumnSetId);
    } else {
      // Add a new column to existing row.
      column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
      // Must explicitly skip the padding column since we've already started
      // adding views.
      layout->SkipColumns(1);
    }

    float expand = input.expand_weight;
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::BASELINE,
                          expand ? expand : 1,
                          views::GridLayout::USE_PREF,
                          0,
                          0);

    ui::ComboboxModel* input_model =
        controller_->ComboboxModelForAutofillType(input.type);
    if (input_model) {
      views::Combobox* combobox = new views::Combobox(input_model);
      combobox->set_listener(this);
      comboboxes->insert(std::make_pair(&input, combobox));
      layout->AddView(combobox);

      for (int i = 0; i < input_model->GetItemCount(); ++i) {
        if (input.initial_value == input_model->GetItemAt(i)) {
          combobox->SetSelectedIndex(i);
          break;
        }
      }
    } else {
      DecoratedTextfield* field = new DecoratedTextfield(
          input.initial_value,
          l10n_util::GetStringUTF16(input.placeholder_text_rid),
          this);

      gfx::Image icon =
          controller_->IconForField(input.type, input.initial_value);
      field->textfield()->SetIcon(icon.AsImageSkia());

      textfields->insert(std::make_pair(&input, field));
      layout->AddView(field);
    }
  }

  return view;
}

void AutofillDialogViews::UpdateSectionImpl(
    DialogSection section,
    bool clobber_inputs) {
  const DetailInputs& updated_inputs =
      controller_->RequestedFieldsForSection(section);
  DetailsGroup* group = GroupForSection(section);

  for (DetailInputs::const_iterator iter = updated_inputs.begin();
       iter != updated_inputs.end(); ++iter) {
    const DetailInput& input = *iter;
    TextfieldMap::iterator text_mapping = group->textfields.find(&input);

    if (text_mapping != group->textfields.end()) {
      views::Textfield* textfield = text_mapping->second->textfield();
      if (textfield->text().empty() || clobber_inputs) {
        textfield->SetText(iter->initial_value);
        textfield->SetIcon(controller_->IconForField(
            input.type, textfield->text()).AsImageSkia());
      }
    }

    ComboboxMap::iterator combo_mapping = group->comboboxes.find(&input);
    if (combo_mapping != group->comboboxes.end()) {
      views::Combobox* combobox = combo_mapping->second;
      for (int i = 0; i < combobox->model()->GetItemCount(); ++i) {
        if (input.initial_value == combobox->model()->GetItemAt(i)) {
          combobox->SetSelectedIndex(i);
          break;
        }
      }
    }
  }

  UpdateDetailsGroupState(*group);
}

void AutofillDialogViews::UpdateDetailsGroupState(const DetailsGroup& group) {
  const SuggestionState& suggestion_state =
      controller_->SuggestionStateForSection(group.section);
  bool show_suggestions = !suggestion_state.text.empty();
  group.suggested_info->SetVisible(show_suggestions);
  group.suggested_info->SetSuggestionText(suggestion_state.text,
                                          suggestion_state.text_style);
  group.suggested_info->SetSuggestionIcon(suggestion_state.icon);
  group.suggested_info->SetEditable(suggestion_state.editable);

  if (!suggestion_state.extra_text.empty()) {
    group.suggested_info->ShowTextfield(
        suggestion_state.extra_text,
        suggestion_state.extra_icon.AsImageSkia());
  }

  group.manual_input->SetVisible(!show_suggestions);

  // Show or hide the "Save in chrome" checkbox. If nothing is in editing mode,
  // hide. If the controller tells us not to show it, likewise hide.
  save_in_chrome_checkbox_->SetVisible(
      controller_->ShouldOfferToSaveInChrome());

  const bool has_menu = !!controller_->MenuModelForSection(group.section);

  if (group.suggested_button)
    group.suggested_button->SetVisible(has_menu);

  if (group.container) {
    group.container->SetForwardMouseEvents(has_menu && show_suggestions);
    group.container->SetVisible(controller_->SectionIsActive(group.section));
    if (group.container->visible())
      ValidateGroup(group, AutofillDialogController::VALIDATE_EDIT);
  }

  ContentsPreferredSizeChanged();
}

template<class T>
void AutofillDialogViews::SetValidityForInput(
    T* input,
    const string16& message) {
  bool invalid = !message.empty();
  input->SetInvalid(invalid);

  if (invalid) {
    validity_map_[input] = message;
    if (!error_bubble_ || !error_bubble_->IsShowing())
      error_bubble_.reset(new ErrorBubble(input, message));
  } else {
    validity_map_.erase(input);
  }
}

bool AutofillDialogViews::ValidateGroup(
    const DetailsGroup& group,
    AutofillDialogController::ValidationType validation_type) {
  DCHECK(group.container->visible());

  scoped_ptr<DetailInput> cvc_input;
  DetailOutputMap detail_outputs;
  typedef std::map<AutofillFieldType, base::Callback<void(const string16&)> >
      FieldMap;
  FieldMap field_map;

  if (group.manual_input->visible()) {
    for (TextfieldMap::const_iterator iter = group.textfields.begin();
         iter != group.textfields.end(); ++iter) {
      detail_outputs[iter->first] = iter->second->textfield()->text();
      field_map[iter->first->type] = base::Bind(
          &AutofillDialogViews::SetValidityForInput<DecoratedTextfield>,
          base::Unretained(this),
          iter->second);
    }
    for (ComboboxMap::const_iterator iter = group.comboboxes.begin();
         iter != group.comboboxes.end(); ++iter) {
      views::Combobox* combobox = iter->second;
      string16 item =
          combobox->model()->GetItemAt(combobox->selected_index());
      detail_outputs[iter->first] = item;
      field_map[iter->first->type] = base::Bind(
          &AutofillDialogViews::SetValidityForInput<views::Combobox>,
          base::Unretained(this),
          iter->second);
    }
  } else if (group.section == SECTION_CC) {
    DecoratedTextfield* decorated_cvc =
        group.suggested_info->decorated_textfield();
    cvc_input.reset(new DetailInput);
    cvc_input->type = CREDIT_CARD_VERIFICATION_CODE;
    detail_outputs[cvc_input.get()] = decorated_cvc->textfield()->text();
    field_map[cvc_input->type] = base::Bind(
        &AutofillDialogViews::SetValidityForInput<DecoratedTextfield>,
        base::Unretained(this),
        decorated_cvc);
  }

  ValidityData invalid_inputs;
  invalid_inputs = controller_->InputsAreValid(detail_outputs, validation_type);
  // Flag invalid fields, removing them from |field_map|.
  for (ValidityData::const_iterator iter =
           invalid_inputs.begin(); iter != invalid_inputs.end(); ++iter) {
    const string16& message = iter->second;
    field_map[iter->first].Run(message);
    field_map.erase(iter->first);
  }

  // The remaining fields in |field_map| are valid. Mark them as such.
  for (FieldMap::iterator iter = field_map.begin(); iter != field_map.end();
       ++iter) {
    iter->second.Run(string16());
  }

  return invalid_inputs.empty();
}

bool AutofillDialogViews::ValidateForm() {
  bool all_valid = true;
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    const DetailsGroup& group = iter->second;
    if (!group.container->visible())
      continue;

    if (!ValidateGroup(group, AutofillDialogController::VALIDATE_FINAL))
      all_valid = false;
  }

  return all_valid;
}

void AutofillDialogViews::TextfieldEditedOrActivated(
    views::Textfield* textfield,
    bool was_edit) {
  DetailsGroup* group = GroupForView(textfield);
  DCHECK(group);

  // Figure out the AutofillFieldType this textfield represents.
  AutofillFieldType type = UNKNOWN_TYPE;
  DecoratedTextfield* decorated = NULL;

  // Look for the input in the manual inputs.
  views::View* ancestor =
      textfield->GetAncestorWithClassName(kDecoratedTextfieldClassName);
  for (TextfieldMap::const_iterator iter = group->textfields.begin();
       iter != group->textfields.end();
       ++iter) {
    decorated = iter->second;
    if (decorated == ancestor) {
      controller_->UserEditedOrActivatedInput(iter->first,
                                              GetWidget()->GetNativeView(),
                                              textfield->GetBoundsInScreen(),
                                              textfield->text(),
                                              was_edit);
      type = iter->first->type;
      break;
    }
  }

  if (ancestor == group->suggested_info->decorated_textfield()) {
    decorated = group->suggested_info->decorated_textfield();
    type = CREDIT_CARD_VERIFICATION_CODE;
  }
  DCHECK_NE(UNKNOWN_TYPE, type);

  // If the field is marked as invalid, check if the text is now valid.
  // Many fields (i.e. CC#) are invalid for most of the duration of editing,
  // so flagging them as invalid prematurely is not helpful. However,
  // correcting a minor mistake (i.e. a wrong CC digit) should immediately
  // result in validation - positive user feedback.
  if (decorated->invalid() && was_edit) {
    bool invalid = !controller_->InputIsValid(type, textfield->text());
    decorated->SetInvalid(invalid);
    if (!invalid && error_bubble_ && error_bubble_->anchor() == decorated)
      error_bubble_.reset();

    // If the field transitioned from invalid to valid, re-validate the group,
    // since inter-field checks become meaningful with valid fields.
    if (!decorated->invalid())
      ValidateGroup(*group, AutofillDialogController::VALIDATE_EDIT);
  }

  gfx::Image icon = controller_->IconForField(type, textfield->text());
  textfield->SetIcon(icon.AsImageSkia());
}

void AutofillDialogViews::ContentsPreferredSizeChanged() {
  if (GetWidget()) {
    GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
    // If the above line does not cause the dialog's size to change, |contents_|
    // may not be laid out. This will trigger a layout only if it's needed.
    contents_->SetBoundsRect(contents_->bounds());
  }
}

AutofillDialogViews::DetailsGroup* AutofillDialogViews::GroupForSection(
    DialogSection section) {
  return &detail_groups_.find(section)->second;
}

AutofillDialogViews::DetailsGroup* AutofillDialogViews::GroupForView(
  views::View* view) {
  DCHECK(view);
  // Textfields are treated slightly differently. For them, inspect
  // the DecoratedTextfield ancestor, not the actual control.
  views::View* ancestor =
      view->GetAncestorWithClassName(kDecoratedTextfieldClassName);

  views::View* control = ancestor ? ancestor : view;
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    DetailsGroup* group = &iter->second;
    if (control->parent() == group->manual_input)
      return group;

    // Textfields need to check a second case, since they can be
    // suggested inputs instead of directly editable inputs. Those are
    // accessed via |suggested_info|.
    if (ancestor &&
        ancestor == group->suggested_info->decorated_textfield()) {
      return group;
    }
  }
  return NULL;
}

views::Textfield* AutofillDialogViews::TextfieldForInput(
    const DetailInput& input) {
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    const DetailsGroup& group = iter->second;
    TextfieldMap::const_iterator text_mapping = group.textfields.find(&input);
    if (text_mapping != group.textfields.end())
      return text_mapping->second->textfield();
  }

  return NULL;
}

views::Combobox* AutofillDialogViews::ComboboxForInput(
    const DetailInput& input) {
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    const DetailsGroup& group = iter->second;
    ComboboxMap::const_iterator combo_mapping = group.comboboxes.find(&input);
    if (combo_mapping != group.comboboxes.end())
      return combo_mapping->second;
  }

  return NULL;
}

AutofillDialogViews::DetailsGroup::DetailsGroup(DialogSection section)
    : section(section),
      container(NULL),
      manual_input(NULL),
      suggested_info(NULL),
      suggested_button(NULL) {}

AutofillDialogViews::DetailsGroup::~DetailsGroup() {}

}  // namespace autofill
