// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include <utility>

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/wallet/wallet_service_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace autofill {

namespace {

// Horizontal padding between text and other elements (in pixels).
const int kAroundTextPadding = 4;

// Size of the triangular mark that indicates an invalid textfield.
const size_t kDogEarSize = 10;

// The space between the top/bottom edge of the notification area and the text.
const size_t kNotificationAreaPadding = 10;

const size_t kAutocheckoutProgressBarWidth = 300;
const size_t kAutocheckoutProgressBarHeight = 11;

const size_t kArrowHeight = 7;
const size_t kArrowWidth = 2 * kArrowHeight;

const char kDecoratedTextfieldClassName[] = "autofill/DecoratedTextfield";
const char kNotificationAreaClassName[] = "autofill/NotificationArea";

// Returns a label that describes a details section.
views::Label* CreateDetailsSectionLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  label->SetFont(label->font().DeriveFont(0, gfx::Font::BOLD));
  // TODO(estade): this should be made to match the native textfield top
  // inset. It's hard to get at, so for now it's hard-coded.
  label->set_border(views::Border::CreateEmptyBorder(4, 0, 0, 0));
  return label;
}

}  // namespace

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

  views::MenuModelAdapter adapter(model);
  menu_runner_.reset(new views::MenuRunner(adapter.CreateMenu()));
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

AutofillDialogViews::NotificationArea::NotificationArea()
    : container_(new views::View()),
      checkbox_(NULL),
      label_(NULL) {
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  box_layout->set_spread_blank_space(true);
  SetLayoutManager(box_layout);

  container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           kNotificationAreaPadding,
                           kNotificationAreaPadding,
                           0));
  AddChildView(container_);
}

AutofillDialogViews::NotificationArea::~NotificationArea() {}

void AutofillDialogViews::NotificationArea::SetNotification(
    const DialogNotification& notification) {
  notification_ = notification;

  // Reserve vertical space for the arrow if necessary.
  const size_t arrow_height = notification_.HasArrow() ? kArrowHeight : 0;
  set_border(views::Border::CreateSolidSidedBorder(
      arrow_height, 0, 0, 0, SK_ColorTRANSPARENT));

  container_->set_background(views::Background::CreateSolidBackground(
      notification_.GetBackgroundColor()));

  const string16& display_text = notification_.display_text();

  if (notification_.HasCheckbox()) {
    if (!checkbox_) {
      checkbox_ = new views::Checkbox(string16());
      static_cast<views::CheckboxNativeThemeBorder*>(checkbox_->border())->
          SetCustomInsets(gfx::Insets());
      checkbox_->set_alignment(views::TextButtonBase::ALIGN_LEFT);
      checkbox_->SetMultiLine(true);
      container_->AddChildView(checkbox_);
    }
    checkbox_->SetEnabledColor(notification_.GetTextColor());
    checkbox_->SetHoverColor(notification_.GetTextColor());
    checkbox_->SetText(display_text);
  } else {
    if (!label_) {
      label_ = new views::Label();
      label_->SetAutoColorReadabilityEnabled(false);
      label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      label_->SetMultiLine(true);
      container_->AddChildView(label_);
    }
    label_->SetEnabledColor(notification_.GetTextColor());
    label_->SetText(display_text);
  }

  if (checkbox_)
    checkbox_->SetVisible(notification_.HasCheckbox() && !display_text.empty());
  if (label_)
    label_->SetVisible(!notification_.HasCheckbox() && !display_text.empty());

  Layout();
}

std::string AutofillDialogViews::NotificationArea::GetClassName() const {
  return kNotificationAreaClassName;
}

void AutofillDialogViews::NotificationArea::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  if (notification_.HasArrow()) {
    const int arrow_half_width = kArrowWidth / 2.0f;
    const int anchor_half_width = arrow_centering_anchor_ ?
        arrow_centering_anchor_->width() / 2.0f : 0;
    const int arrow_middle =
        GetMirroredXInView(width() - anchor_half_width - arrow_half_width);

    SkPath arrow;
    arrow.moveTo(arrow_middle - arrow_half_width, kArrowHeight);
    arrow.lineTo(arrow_middle + arrow_half_width, kArrowHeight);
    arrow.lineTo(arrow_middle, 0);
    arrow.close();
    canvas->ClipPath(arrow);
    canvas->DrawColor(notification_.GetBackgroundColor());
  }
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
  column_set->AddPaddingColumn(0, 15);
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

void AutofillDialogViews::SectionContainer::SetForwardMouseEvents(
    bool forward) {
  forward_mouse_events_ = forward;
  if (!forward)
    set_background(NULL);
}

void AutofillDialogViews::SectionContainer::OnMouseEntered(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  // TODO(estade): use the correct color.
  set_background(views::Background::CreateSolidBackground(SK_ColorLTGRAY));
  proxy_button_->OnMouseEntered(ProxyEvent(event));
  SchedulePaint();
}

void AutofillDialogViews::SectionContainer::OnMouseExited(
    const ui::MouseEvent& event) {
  if (!forward_mouse_events_)
    return;

  set_background(NULL);
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
      label_container_(new views::View()),
      decorated_(
          new DecoratedTextfield(string16(), string16(), autofill_dialog)) {
  // Label and icon.
  label_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                           kAroundTextPadding));
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
  views::Link* edit_link = new views::Link(edit_label);
  edit_link->set_listener(autofill_dialog);
  edit_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  edit_link->SetUnderline(false);

  // This container prevents the edit link from being horizontally stretched.
  views::View* link_container = new views::View();
  link_container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  link_container->AddChildView(edit_link);
  AddChildView(link_container);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
}

AutofillDialogViews::SuggestionView::~SuggestionView() {}

void AutofillDialogViews::SuggestionView::SetSuggestionText(
    const string16& text) {
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
      did_submit_(false),
      window_(NULL),
      contents_(NULL),
      notification_area_(NULL),
      use_billing_for_shipping_(NULL),
      account_chooser_(NULL),
      sign_in_container_(NULL),
      cancel_sign_in_(NULL),
      sign_in_webview_(NULL),
      main_container_(NULL),
      button_strip_extra_view_(NULL),
      save_in_chrome_checkbox_(NULL),
      autocheckout_progress_bar_view_(NULL),
      autocheckout_progress_bar_(NULL),
      focus_manager_(NULL) {
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

  // |notification_area_| could be NULL if |Show()| was never called.
  if (notification_area_)
    notification_area_->set_arrow_centering_anchor(NULL);
}

void AutofillDialogViews::Show() {
  InitChildViews();
  UpdateAccountChooser();
  UpdateNotificationArea();

  // Ownership of |contents_| is handed off by this call. The widget will take
  // care of deleting itself after calling DeleteDelegate().
  window_ = ConstrainedWindowViews::Create(controller_->web_contents(), this);
  focus_manager_ = window_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);
}

void AutofillDialogViews::Hide() {
  if (window_)
    window_->Close();
}

void AutofillDialogViews::UpdateAccountChooser() {
  account_chooser_->Update();
}

void AutofillDialogViews::UpdateNotificationArea() {
  DCHECK(notification_area_);

  bool had_checkbox = notification_area_->checkbox() != NULL;

  // TODO(dbeam): support multiple notifications (see: http://crbug.com/174814).
  const std::vector<DialogNotification>& notifications =
      controller_->CurrentNotifications();
  notification_area_->SetNotification(
      !notifications.empty() ? notifications[0] : DialogNotification());

  // "Save details in Wallet" should be checked by default the first time shown.
  if (!had_checkbox && notification_area_->checkbox())
    notification_area_->checkbox()->SetChecked(true);

  ContentsPreferredSizeChanged();
}

void AutofillDialogViews::UpdateSection(DialogSection section) {
  const DetailInputs& updated_inputs =
      controller_->RequestedFieldsForSection(section);
  DetailsGroup* group = GroupForSection(section);

  for (DetailInputs::const_iterator iter = updated_inputs.begin();
       iter != updated_inputs.end(); ++iter) {
    const DetailInput& input = *iter;
    TextfieldMap::iterator text_mapping = group->textfields.find(&input);
    if (text_mapping != group->textfields.end())
      text_mapping->second->textfield()->SetText(iter->autofilled_value);

    ComboboxMap::iterator combo_mapping = group->comboboxes.find(&input);
    if (combo_mapping != group->comboboxes.end()) {
      views::Combobox* combobox = combo_mapping->second;
      for (int i = 0; i < combobox->model()->GetItemCount(); ++i) {
        if (input.autofilled_value == combobox->model()->GetItemAt(i)) {
          combobox->SetSelectedIndex(i);
          break;
        }
      }
    }
  }

  UpdateDetailsGroupState(*group);
}

void AutofillDialogViews::GetUserInput(DialogSection section,
                                       DetailOutputMap* output) {
  DetailsGroup* group = GroupForSection(section);
  for (TextfieldMap::iterator it = group->textfields.begin();
       it != group->textfields.end(); ++it) {
    output->insert(std::make_pair(it->first, it->second->textfield()->text()));
  }
}

string16 AutofillDialogViews::GetCvc() {
  return GroupForSection(SECTION_CC)->suggested_info->decorated_textfield()->
      textfield()->text();
}

bool AutofillDialogViews::UseBillingForShipping() {
  return use_billing_for_shipping_->checked();
}

bool AutofillDialogViews::SaveDetailsInWallet() {
  return notification_area_->checkbox() &&
         notification_area_->checkbox()->visible() &&
         notification_area_->checkbox()->checked();
}

bool AutofillDialogViews::SaveDetailsLocally() {
  return save_in_chrome_checkbox_->checked();
}

const content::NavigationController& AutofillDialogViews::ShowSignIn() {
  // TODO(abodenha) Also hide Submit and Cancel buttons.
  // See http://crbug.com/165193
  // TODO(abodenha) We should be able to use the WebContents of the WebView
  // to navigate instead of LoadInitialURL.  Figure out why it doesn't work.

  sign_in_webview_->LoadInitialURL(wallet::GetSignInUrl());
  // TODO(abodenha) Resize the dialog to avoid the need for a scroll bar on
  // sign in. See http://crbug.com/169286
  sign_in_webview_->SetPreferredSize(contents_->GetPreferredSize());
  main_container_->SetVisible(false);
  sign_in_container_->SetVisible(true);
  contents_->Layout();
  return sign_in_webview_->web_contents()->GetController();
}

void AutofillDialogViews::HideSignIn() {
  sign_in_container_->SetVisible(false);
  main_container_->SetVisible(true);
}

void AutofillDialogViews::UpdateProgressBar(double value) {
  autocheckout_progress_bar_->SetValue(value);
}

void AutofillDialogViews::ModelChanged() {
  menu_runner_.reset();

  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    UpdateDetailsGroupState(iter->second);
  }
}

void AutofillDialogViews::SubmitForTesting() {
  if (Accept())
    Hide();
}

void AutofillDialogViews::CancelForTesting() {
  if (Cancel())
    Hide();
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
  controller_->ViewClosed(did_submit_ ? ACTION_SUBMIT : ACTION_CANCEL);
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

string16 AutofillDialogViews::GetDialogButtonLabel(ui::DialogButton button)
    const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonText() : controller_->CancelButtonText();
}

bool AutofillDialogViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  return true;
}

views::View* AutofillDialogViews::CreateExtraView() {
  return button_strip_extra_view_;
}

views::View* AutofillDialogViews::CreateFootnoteView() {
  // TODO(estade): add a view to contain the terms of service.
  return NULL;
}

bool AutofillDialogViews::Cancel() {
  return true;
}

bool AutofillDialogViews::Accept() {
  if (!ValidateForm())
    return false;

  did_submit_ = true;
  return true;
}

void AutofillDialogViews::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == use_billing_for_shipping_) {
    UpdateDetailsGroupState(*GroupForSection(SECTION_SHIPPING));
  } else if (sender == cancel_sign_in_) {
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

    views::MenuModelAdapter adapter(
        controller_->MenuModelForSection(group->section));
    menu_runner_.reset(new views::MenuRunner(adapter.CreateMenu()));

    // Ignore the result since we don't need to handle a deleted menu specially.
    ignore_result(
        menu_runner_->RunMenuAt(sender->GetWidget(),
                                NULL,
                                group->suggested_button->GetBoundsInScreen(),
                                views::MenuItemView::TOPRIGHT,
                                0));
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
    views::View* focused_now) {}

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
  combobox->SetInvalid(false);
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
  autocheckout_progress_bar_view_->SetVisible(false);

  views::Label* progress_bar_label = new views::Label();
  progress_bar_label->SetText(controller_->ProgressBarText());
  progress_bar_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  autocheckout_progress_bar_view_->AddChildView(progress_bar_label);

  autocheckout_progress_bar_ = new AutocheckoutProgressBar();
  autocheckout_progress_bar_view_->AddChildView(autocheckout_progress_bar_);

  button_strip_extra_view_->AddChildView(autocheckout_progress_bar_view_);

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
  cancel_sign_in_ = new views::TextButton(this,
                                          controller_->CancelSignInText());
  sign_in_container_->AddChildView(cancel_sign_in_);
  sign_in_container_->AddChildView(sign_in_webview_);
  return sign_in_container_;
}

views::View* AutofillDialogViews::CreateMainContainer() {
  main_container_ = new views::View();
  views::GridLayout* layout = new views::GridLayout(main_container_);
  main_container_->SetLayoutManager(layout);

  const int single_column_set = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->StartRow(0, single_column_set);
  // TODO(abodenha) Create a chooser control to allow account selection.
  // See http://crbug.com/169858
  account_chooser_ = new AccountChooser(controller_);
  // TODO(estade): |account_chooser_| should be in the title bar.
  layout->AddView(account_chooser_);

  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kRelatedControlVerticalSpacing);
  notification_area_ = new NotificationArea();
  notification_area_->set_arrow_centering_anchor(account_chooser_);
  layout->AddView(notification_area_);

  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(CreateDetailsContainer());
  return main_container_;
}

views::View* AutofillDialogViews::CreateDetailsContainer() {
  views::View* view = new views::View();
  // A box layout is used because it respects widget visibility.
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           views::kRelatedControlVerticalSpacing));
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    CreateDetailsSection(iter->second.section);
    view->AddChildView(iter->second.container);
  }

  return view;
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
  column_set->AddColumn(views::GridLayout::CENTER,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  layout->StartRow(0, kColumnSetId);

  // The |info_view| holds |manual_inputs| and |suggested_info|, allowing the
  // dialog toggle which is shown.
  views::View* info_view = new views::View();
  info_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  if (section == SECTION_SHIPPING) {
    use_billing_for_shipping_ =
        new views::Checkbox(controller_->UseBillingForShippingText());
    use_billing_for_shipping_->SetChecked(true);
    use_billing_for_shipping_->set_listener(this);
    info_view->AddChildView(use_billing_for_shipping_);
  }

  views::View* manual_inputs = InitInputsView(section);
  info_view->AddChildView(manual_inputs);
  SuggestionView* suggested_info =
      new SuggestionView(controller_->EditSuggestionText(), this);
  info_view->AddChildView(suggested_info);
  layout->AddView(info_view);

  if (section == SECTION_CC) {
    // TODO(estade): don't hardcode this string.
    suggested_info->ShowTextfield(
        ASCIIToUTF16("CVC"),
        controller_->IconForField(CREDIT_CARD_VERIFICATION_CODE,
                                  string16()).AsImageSkia());
  }

  // TODO(estade): Fix the appearance of this button.
  views::ImageButton* menu_button = new views::ImageButton(this);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  menu_button->SetImage(views::CustomButton::STATE_NORMAL,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON));
  menu_button->SetImage(views::CustomButton::STATE_PRESSED,
      rb.GetImageSkiaNamed(IDR_AUTOFILL_DIALOG_MENU_BUTTON_P));
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
        if (input.autofilled_value == input_model->GetItemAt(i)) {
          combobox->SetSelectedIndex(i);
          break;
        }
      }
    } else {
      DecoratedTextfield* field = new DecoratedTextfield(
          input.autofilled_value,
          l10n_util::GetStringUTF16(input.placeholder_text_rid),
          this);

      gfx::Image icon =
          controller_->IconForField(input.type, input.autofilled_value);
      field->textfield()->SetIcon(icon.AsImageSkia());

      textfields->insert(std::make_pair(&input, field));
      layout->AddView(field);
    }
  }

  return view;
}

void AutofillDialogViews::UpdateDetailsGroupState(const DetailsGroup& group) {
  string16 suggestion_text =
      controller_->SuggestionTextForSection(group.section);
  bool show_suggestions = !suggestion_text.empty();
  group.suggested_info->SetVisible(show_suggestions);
  group.suggested_info->SetSuggestionText(suggestion_text);
  gfx::Image icon = controller_->SuggestionIconForSection(group.section);
  group.suggested_info->SetSuggestionIcon(icon);

  if (group.section == SECTION_SHIPPING) {
    bool show_checkbox = !show_suggestions;
    // When the checkbox is going from hidden to visible, it's because the
    // user clicked "Enter new address". Reset the checkbox to unchecked in this
    // case.
    if (show_checkbox && !use_billing_for_shipping_->visible())
      use_billing_for_shipping_->SetChecked(false);

    use_billing_for_shipping_->SetVisible(show_checkbox);
    group.manual_input->SetVisible(
        show_checkbox && !use_billing_for_shipping_->checked());
  } else {
    group.manual_input->SetVisible(!show_suggestions);
  }

  // Show or hide the "Save in chrome" checkbox. If nothing is in editing mode,
  // hide. If the controller tells us not to show it, likewise hide.
  save_in_chrome_checkbox_->SetVisible(
      controller_->ShouldOfferToSaveInChrome() && AtLeastOneSectionIsEditing());

  if (group.container) {
    group.container->SetForwardMouseEvents(show_suggestions);
    group.container->SetVisible(controller_->SectionIsActive(group.section));
  }

  ContentsPreferredSizeChanged();
}

bool AutofillDialogViews::AtLeastOneSectionIsEditing() {
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    if (iter->second.manual_input && iter->second.manual_input->visible())
      return true;
  }

  return false;
}

bool AutofillDialogViews::ValidateForm() {
  bool all_valid = true;
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    DetailsGroup* group = &iter->second;
    if (!group->container->visible())
      continue;

    if (group->manual_input->visible()) {
      for (TextfieldMap::iterator iter = group->textfields.begin();
           iter != group->textfields.end(); ++iter) {
        if (!controller_->InputIsValid(iter->first->type,
                                       iter->second->textfield()->text())) {
          iter->second->SetInvalid(true);
          all_valid = false;
        }
      }

      for (ComboboxMap::iterator iter = group->comboboxes.begin();
           iter != group->comboboxes.end(); ++iter) {
        const DetailInput* input = iter->first;
        views::Combobox* combobox = iter->second;
        string16 item =
            combobox->model()->GetItemAt(combobox->selected_index());
        if (!controller_->InputIsValid(input->type, item)) {
          combobox->SetInvalid(true);
          all_valid = false;
        }
      }
    } else if (group->section == SECTION_CC) {
      DecoratedTextfield* decorated_cvc =
          group->suggested_info->decorated_textfield();
      if (!controller_->InputIsValid(CREDIT_CARD_VERIFICATION_CODE,
                                     decorated_cvc->textfield()->text())) {
        decorated_cvc->SetInvalid(true);
        all_valid = false;
      }
    }
  }

  return all_valid;
}

void AutofillDialogViews::TextfieldEditedOrActivated(
    views::Textfield* textfield,
    bool was_edit) {
  views::View* ancestor =
      textfield->GetAncestorWithClassName(kDecoratedTextfieldClassName);
  DetailsGroup* group = NULL;
  for (DetailGroupMap::iterator iter = detail_groups_.begin();
       iter != detail_groups_.end(); ++iter) {
    if (ancestor->parent() == iter->second.manual_input ||
        ancestor == iter->second.suggested_info->decorated_textfield()) {
      group = &iter->second;
      break;
    }
  }
  DCHECK(group);

  // Figure out the AutofillFieldType this textfield represents.
  AutofillFieldType type = UNKNOWN_TYPE;
  DecoratedTextfield* decorated = NULL;

  // Look for the input in the manual inputs.
  for (TextfieldMap::const_iterator iter = group->textfields.begin();
       iter != group->textfields.end();
       ++iter) {
    decorated = iter->second;
    if (decorated == ancestor) {
      controller_->UserEditedOrActivatedInput(iter->first,
                                              group->section,
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
  if (decorated->invalid() && was_edit)
    decorated->SetInvalid(!controller_->InputIsValid(type, textfield->text()));

  gfx::Image icon = controller_->IconForField(type, textfield->text());
  textfield->SetIcon(icon.AsImageSkia());
}

void AutofillDialogViews::ContentsPreferredSizeChanged() {
  if (GetWidget())
    GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

AutofillDialogViews::DetailsGroup* AutofillDialogViews::GroupForSection(
    DialogSection section) {
  return &detail_groups_.find(section)->second;
}

AutofillDialogViews::DetailsGroup::DetailsGroup(DialogSection section)
    : section(section),
      container(NULL),
      manual_input(NULL),
      suggested_info(NULL),
      suggested_button(NULL) {}

AutofillDialogViews::DetailsGroup::~DetailsGroup() {}

}  // namespace autofill
