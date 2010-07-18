// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/views/autofill_profiles_view_win.h"

#include <vsstyle.h>
#include <vssym32.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/list_background.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/native_theme_win.h"
#include "gfx/size.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/border.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/scroll_view.h"
#include "views/controls/separator.h"
#include "views/controls/table/table_view.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace {

// padding on the sides of AutoFill settings dialog.
const int kDialogPadding = 7;

// Insets for subview controls.
const int kSubViewHorizotalInsets = 18;
const int kSubViewVerticalInsets = 5;

};  // namespace

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, static data:
AutoFillProfilesView* AutoFillProfilesView::instance_ = NULL;

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, public:
AutoFillProfilesView::AutoFillProfilesView(
    AutoFillDialogObserver* observer,
    PersonalDataManager* personal_data_manager,
    Profile* profile,
    PrefService* preferences,
    AutoFillProfile* imported_profile,
    CreditCard* imported_credit_card)
    : observer_(observer),
      personal_data_manager_(personal_data_manager),
      profile_(profile),
      preferences_(preferences),
      enable_auto_fill_button_(NULL),
      add_address_button_(NULL),
      add_credit_card_button_(NULL),
      edit_button_(NULL),
      remove_button_(NULL),
      scroll_view_(NULL),
      focus_manager_(NULL),
      billing_model_(true),
      child_dialog_opened_(false) {
  DCHECK(preferences_);
  if (imported_profile) {
    profiles_set_.push_back(EditableSetInfo(imported_profile));
  }
  if (imported_credit_card) {
    credit_card_set_.push_back(
        EditableSetInfo(imported_credit_card));
  }
}

AutoFillProfilesView::~AutoFillProfilesView() {
  // Clear model as it gets deleted before the view.
  if (scroll_view_)
    scroll_view_->SetModel(NULL);

  // Removes observer if we are observing Profile load. Does nothing otherwise.
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
}

// TODO: get rid of imported_profile and imported_credit_card.
int AutoFillProfilesView::Show(gfx::NativeWindow parent,
                               AutoFillDialogObserver* observer,
                               PersonalDataManager* personal_data_manager,
                               Profile* profile,
                               PrefService* preferences,
                               AutoFillProfile* imported_profile,
                               CreditCard* imported_credit_card) {
  if (!instance_) {
    instance_ = new AutoFillProfilesView(observer, personal_data_manager,
        profile, preferences, imported_profile, imported_credit_card);

    // |instance_| will get deleted once Close() is called.
    views::Window::CreateChromeWindow(parent, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible())
    instance_->window()->Show();
  else
    instance_->window()->Activate();
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, protected:
void AutoFillProfilesView::AddClicked(int group_type) {
  std::vector<EditableSetInfo>::iterator it = profiles_set_.end();
  int added_item_index = -1;
  if (group_type == ContentListTableModel::kAddressGroup) {
    AutoFillProfile address(std::wstring(), 0);
    profiles_set_.push_back(EditableSetInfo(&address));
    added_item_index = profiles_set_.size() - 1;
    it = profiles_set_.begin() + added_item_index;
  } else if (group_type == ContentListTableModel::kCreditCardGroup) {
    CreditCard credit_card(std::wstring(), 0);
    credit_card_set_.push_back(EditableSetInfo(&credit_card));
    added_item_index = profiles_set_.size() + credit_card_set_.size() - 1;
    it = credit_card_set_.begin() + (credit_card_set_.size() - 1);
  } else {
    NOTREACHED();
  }

  EditableSetViewContents *edit_view = new
      EditableSetViewContents(this, &billing_model_, true, it);
  views::Window::CreateChromeWindow(window()->GetNativeWindow(), gfx::Rect(),
                                    edit_view);
  edit_view->window()->Show();
}

void AutoFillProfilesView::EditClicked() {
  DCHECK(scroll_view_);
  int index = scroll_view_->FirstSelectedRow();
  DCHECK(index >= 0);
  DCHECK(index < static_cast<int>(profiles_set_.size() +
      credit_card_set_.size()));
  std::vector<EditableSetInfo>::iterator it;
  if (index < static_cast<int>(profiles_set_.size()))
    it = profiles_set_.begin() + index;
  else
    it = credit_card_set_.begin() + (index - profiles_set_.size());

  EditableSetViewContents *edit_view = new
      EditableSetViewContents(this, &billing_model_, false, it);
  views::Window::CreateChromeWindow(window()->GetNativeWindow(), gfx::Rect(),
                                    edit_view);
  edit_view->window()->Show();
}

void AutoFillProfilesView::DeleteClicked() {
  DCHECK(scroll_view_);
  DCHECK(table_model_.get());
  DCHECK_GT(scroll_view_->SelectedRowCount(), 0);
  int last_view_row = -1;
  for (views::TableView::iterator i = scroll_view_->SelectionBegin();
       i != scroll_view_->SelectionEnd(); ++i) {
    last_view_row = scroll_view_->ModelToView(*i);
    table_model_->RemoveItem(*i);
  }
  if (last_view_row >= table_model_->RowCount())
    last_view_row = table_model_->RowCount() - 1;
  if (last_view_row >= 0)
    scroll_view_->Select(scroll_view_->ViewToModel(last_view_row));
  UpdateButtonState();
}

void AutoFillProfilesView::UpdateButtonState() {
  DCHECK(personal_data_manager_);
  DCHECK(scroll_view_);
  DCHECK(add_address_button_);
  DCHECK(add_credit_card_button_);
  DCHECK(edit_button_);
  DCHECK(remove_button_);
  bool autofill_enabled = preferences_->GetBoolean(prefs::kAutoFillEnabled);
  scroll_view_->SetEnabled(autofill_enabled);
  add_address_button_->SetEnabled(personal_data_manager_->IsDataLoaded() &&
                                  !child_dialog_opened_ && autofill_enabled);
  add_credit_card_button_->SetEnabled(personal_data_manager_->IsDataLoaded() &&
                                      !child_dialog_opened_ &&
                                      autofill_enabled);

  int selected_row_count = scroll_view_->SelectedRowCount();
  edit_button_->SetEnabled(selected_row_count == 1 && !child_dialog_opened_ &&
                           autofill_enabled);
  remove_button_->SetEnabled(selected_row_count > 0 && !child_dialog_opened_ &&
                             autofill_enabled);
}

void AutoFillProfilesView::UpdateProfileLabels() {
  std::vector<AutoFillProfile*> profiles;
  profiles.resize(profiles_set_.size());
  for (size_t i = 0; i < profiles_set_.size(); ++i) {
    profiles[i] = &(profiles_set_[i].address);
  }
  AutoFillProfile::AdjustInferredLabels(&profiles);
}

void AutoFillProfilesView::ChildWindowOpened() {
  child_dialog_opened_ = true;
  UpdateButtonState();
}

void AutoFillProfilesView::ChildWindowClosed() {
  child_dialog_opened_ = false;
  UpdateButtonState();
}

SkBitmap* AutoFillProfilesView::GetWarningBimap(bool good) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetBitmapNamed(good ? IDR_INPUT_GOOD : IDR_INPUT_ALERT);
}


/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::View implementations
void AutoFillProfilesView::Layout() {
  View::Layout();
}

gfx::Size AutoFillProfilesView::GetPreferredSize() {
  return views::Window::GetLocalizedContentsSize(
      IDS_AUTOFILL_DIALOG_WIDTH_CHARS,
      IDS_AUTOFILL_DIALOG_HEIGHT_LINES);
}

void AutoFillProfilesView::ViewHierarchyChanged(bool is_add,
                                                views::View* parent,
                                                views::View* child) {
  if (is_add && child == this)
    Init();
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::DialogDelegate implementations:
int AutoFillProfilesView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring AutoFillProfilesView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
  case MessageBoxFlags::DIALOGBUTTON_CANCEL:
    return std::wstring();
  default:
    break;
  }
  NOTREACHED();
  return std::wstring();
}

views::View* AutoFillProfilesView::GetExtraView() {
  // The dialog sizes the extra view to fill the entire available space.
  // We use a container to lay it out properly.
  views::View* link_container = new views::View();
  views::GridLayout* layout = new views::GridLayout(link_container);
  link_container->SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, kDialogPadding);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  views::Link* link = new views::Link(
      l10n_util::GetString(IDS_AUTOFILL_LEARN_MORE));
  link->SetController(this);
  layout->AddView(link);

  return link_container;
}

bool AutoFillProfilesView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
  case MessageBoxFlags::DIALOGBUTTON_OK:
  case MessageBoxFlags::DIALOGBUTTON_CANCEL:
    return true;
  default:
    break;
  }
  NOTREACHED();
  return false;
}


std::wstring AutoFillProfilesView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_AUTOFILL_OPTIONS);
}

void AutoFillProfilesView::WindowClosing() {
  DCHECK(focus_manager_);
  focus_manager_->RemoveFocusChangeListener(this);
  instance_ = NULL;
}

views::View* AutoFillProfilesView::GetContentsView() {
  return this;
}

bool AutoFillProfilesView::Cancel() {
  // No way to cancel - we save back all the info always.
  return Accept();
}

bool AutoFillProfilesView::Accept() {
  DCHECK(observer_);
  std::vector<AutoFillProfile> profiles;
  profiles.reserve(profiles_set_.size());
  std::vector<EditableSetInfo>::iterator it;
  for (it = profiles_set_.begin(); it != profiles_set_.end(); ++it) {
    profiles.push_back(it->address);
  }
  std::vector<CreditCard> credit_cards;
  credit_cards.reserve(credit_card_set_.size());
  for (it = credit_card_set_.begin(); it != credit_card_set_.end(); ++it) {
    credit_cards.push_back(it->credit_card);
  }
  DCHECK(preferences_);
  observer_->OnAutoFillDialogApply(&profiles, &credit_cards);
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::ButtonListener implementations:
void AutoFillProfilesView::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
  if (sender == add_address_button_) {
    AddClicked(ContentListTableModel::kAddressGroup);
  } else if (sender == add_credit_card_button_) {
    AddClicked(ContentListTableModel::kCreditCardGroup);
  } else if (sender == edit_button_) {
    EditClicked();
  } else if (sender == remove_button_) {
    DeleteClicked();
  } else if (sender == enable_auto_fill_button_) {
    bool enabled = enable_auto_fill_button_->checked();
    UserMetricsAction action(enabled ? "Options_FormAutofill_Enable" :
                                       "Options_FormAutofill_Disable");
    UserMetrics::RecordAction(action, profile_);
    preferences_->SetBoolean(prefs::kAutoFillEnabled, enabled);
    preferences_->ScheduleSavePersistentPrefs();
    UpdateButtonState();
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::LinkController implementations:
void AutoFillProfilesView::LinkActivated(views::Link* source, int event_flags) {
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(GURL(kAutoFillLearnMoreUrl), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::TYPED);
}


/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::FocusChangeListener implementations:
void AutoFillProfilesView::FocusWillChange(views::View* focused_before,
                                           views::View* focused_now) {
  if (focused_now) {
    focused_now->ScrollRectToVisible(gfx::Rect(focused_now->width(),
                                     focused_now->height()));
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::TableViewObserver implementations:
void AutoFillProfilesView::OnSelectionChanged() {
  UpdateButtonState();
}

void AutoFillProfilesView::OnDoubleClick() {
  if (!child_dialog_opened_)
    EditClicked();
}


/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, PersonalDataManager::Observer implementation.
void  AutoFillProfilesView::OnPersonalDataLoaded() {
  personal_data_manager_->RemoveObserver(this);
  GetData();
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, private:
void AutoFillProfilesView::Init() {
  GetData();

  enable_auto_fill_button_ = new views::Checkbox(
      l10n_util::GetString(IDS_OPTIONS_AUTOFILL_ENABLE));
  enable_auto_fill_button_->set_listener(this);
  enable_auto_fill_button_->SetChecked(
      preferences_->GetBoolean(prefs::kAutoFillEnabled));

  billing_model_.set_address_labels(&profiles_set_);

  table_model_.reset(new ContentListTableModel(&profiles_set_,
                                               &credit_card_set_));
  std::vector<TableColumn> columns;
  columns.resize(1);
  columns[0] = TableColumn(IDS_AUTOFILL_LIST_HEADER_SUMMARY,
                           TableColumn::LEFT, -1, .67f);
  columns.back().sortable = false;

  scroll_view_ = new views::TableView(table_model_.get(), columns,
                                      views::TEXT_ONLY, false, true, true);
  scroll_view_->SetObserver(this);

  add_address_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_AUTOFILL_ADD_ADDRESS_BUTTON));
  add_credit_card_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_AUTOFILL_ADD_CREDITCARD_BUTTON));
  edit_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_AUTOFILL_EDIT_BUTTON));
  remove_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_AUTOFILL_DELETE_BUTTON));

  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int table_with_buttons_column_view_set_id = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(table_with_buttons_column_view_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, table_with_buttons_column_view_set_id);
  layout->AddView(enable_auto_fill_button_, 3, 1, views::GridLayout::FILL,
                  views::GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, table_with_buttons_column_view_set_id);
  layout->AddView(scroll_view_, 1, 8, views::GridLayout::FILL,
                  views::GridLayout::FILL);
  layout->AddView(add_address_button_);

  layout->StartRowWithPadding(0, table_with_buttons_column_view_set_id, 0,
                              kRelatedControlVerticalSpacing);
  layout->SkipColumns(2);
  layout->AddView(add_credit_card_button_);

  layout->StartRowWithPadding(0, table_with_buttons_column_view_set_id, 0,
                              kRelatedControlVerticalSpacing);
  layout->SkipColumns(2);
  layout->AddView(edit_button_);

  layout->StartRowWithPadding(0, table_with_buttons_column_view_set_id, 0,
                              kRelatedControlVerticalSpacing);
  layout->SkipColumns(2);
  layout->AddView(remove_button_);

  layout->AddPaddingRow(1, table_with_buttons_column_view_set_id);


  focus_manager_ = GetFocusManager();
  DCHECK(focus_manager_);
  focus_manager_->AddFocusChangeListener(this);

  UpdateButtonState();
}

void AutoFillProfilesView::GetData() {
  if (!personal_data_manager_->IsDataLoaded()) {
    personal_data_manager_->SetObserver(this);
    return;
  }
  bool imported_data_present = !profiles_set_.empty() ||
                               !credit_card_set_.empty();
  if (!imported_data_present) {
    profiles_set_.reserve(personal_data_manager_->profiles().size());
    for (std::vector<AutoFillProfile*>::const_iterator address_it =
         personal_data_manager_->profiles().begin();
         address_it != personal_data_manager_->profiles().end();
         ++address_it) {
      profiles_set_.push_back(EditableSetInfo(*address_it));
    }
  }
  UpdateProfileLabels();

  if (!imported_data_present) {
    credit_card_set_.reserve(personal_data_manager_->credit_cards().size());
    for (std::vector<CreditCard*>::const_iterator cc_it =
         personal_data_manager_->credit_cards().begin();
         cc_it != personal_data_manager_->credit_cards().end();
         ++cc_it) {
      credit_card_set_.push_back(EditableSetInfo(*cc_it));
    }
  }
  if (table_model_.get())
    table_model_->Refresh();

  // Update state only if buttons already created.
  if (add_address_button_) {
    UpdateButtonState();
  }
}

bool AutoFillProfilesView::IsDataReady() const {
  return personal_data_manager_->IsDataLoaded();
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::PhoneSubView, public:
AutoFillProfilesView::PhoneSubView::PhoneSubView(
    AutoFillProfilesView* autofill_view,
    views::Label* label,
    views::Textfield* text_phone)
    : autofill_view_(autofill_view),
      label_(label),
      text_phone_(text_phone),
      phone_warning_button_(NULL),
      last_state_(false) {
  DCHECK(label_);
  DCHECK(text_phone_);
}

void AutoFillProfilesView::PhoneSubView::ContentsChanged(
    views::Textfield* sender, const string16& new_contents) {
  if (sender == text_phone_) {
    UpdateButtons();
  }
}

bool AutoFillProfilesView::PhoneSubView::IsValid() const {
  if (text_phone_) {
    string16 phone = text_phone_->text();
    if (phone.empty())
      return true;

    // Try to parse it.
    string16 country, city, stripped_phone;
    return PhoneNumber::ParsePhoneNumber(phone, &country, &city,
                                         &stripped_phone);
  }
  return false;
}

void AutoFillProfilesView::PhoneSubView::UpdateButtons() {
  if (phone_warning_button_) {
    SkBitmap* image = text_phone_->text().empty() ? NULL :
        autofill_view_->GetWarningBimap(IsValid());
    phone_warning_button_->SetImage(views::CustomButton::BS_NORMAL, image);
    if (last_state_ != IsValid()) {
      last_state_ = IsValid();
      SchedulePaint();
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::PhoneSubView, protected:
/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::PhoneSubView, views::View implementations
void AutoFillProfilesView::PhoneSubView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  if (is_add && this == child) {
    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);
    const int two_column_fill_view_set_id = 0;
    views::ColumnSet* column_set =
        layout->AddColumnSet(two_column_fill_view_set_id);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
        views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
        views::GridLayout::USE_PREF, 0, 0);
    layout->StartRow(0, two_column_fill_view_set_id);
    layout->AddView(label_, 3, 1);
    layout->StartRow(0, two_column_fill_view_set_id);
    text_phone_->set_default_width_in_chars(15);
    layout->AddView(text_phone_);
    phone_warning_button_ = new views::ImageButton(this);
    // Set default size of the image.
    SkBitmap* image = autofill_view_->GetWarningBimap(true);
    phone_warning_button_->SetPreferredSize(gfx::Size(image->width(),
                                                      image->height()));
    phone_warning_button_->SetEnabled(false);
    phone_warning_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                             views::ImageButton::ALIGN_MIDDLE);
    layout->AddView(phone_warning_button_);
    UpdateButtons();
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, static data:
AutoFillProfilesView::EditableSetViewContents::TextFieldToAutoFill
    AutoFillProfilesView::EditableSetViewContents::address_fields_[] = {
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FULL_NAME,
    NAME_FULL },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_COMPANY, COMPANY_NAME },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_EMAIL, EMAIL_ADDRESS },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_LINE_1,
    ADDRESS_HOME_LINE1 },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_LINE_2,
    ADDRESS_HOME_LINE2 },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_CITY,
    ADDRESS_HOME_CITY },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_STATE,
    ADDRESS_HOME_STATE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_ZIP,
    ADDRESS_HOME_ZIP },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_ADDRESS_COUNTRY,
    ADDRESS_HOME_COUNTRY },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_PHONE,
    PHONE_HOME_NUMBER },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FAX_PHONE,
    PHONE_FAX_NUMBER },
};

AutoFillProfilesView::EditableSetViewContents::TextFieldToAutoFill
    AutoFillProfilesView::EditableSetViewContents::credit_card_fields_[] = {
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_NAME,
    CREDIT_CARD_NAME },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_NUMBER,
    CREDIT_CARD_NUMBER },
};

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, public:
AutoFillProfilesView::EditableSetViewContents::EditableSetViewContents(
    AutoFillProfilesView* observer,
    AddressComboBoxModel* billing_model,
    bool new_item,
    std::vector<EditableSetInfo>::iterator field_set)
    : editable_fields_set_(field_set),
      temporary_info_(*editable_fields_set_),
      observer_(observer),
      billing_model_(billing_model),
      combo_box_billing_(NULL),
      new_item_(new_item) {
  ZeroMemory(text_fields_, sizeof(text_fields_));
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, protected:
/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, views::View implementations
void AutoFillProfilesView::EditableSetViewContents::Layout() {
  View::Layout();
}

gfx::Size AutoFillProfilesView::EditableSetViewContents::GetPreferredSize() {
  if (temporary_info_.is_address) {
    return views::Window::GetLocalizedContentsSize(
        IDS_AUTOFILL_DIALOG_EDIT_ADDRESS_WIDTH_CHARS,
        IDS_AUTOFILL_DIALOG_EDIT_ADDRESS_HEIGHT_LINES);
  } else {
    return views::Window::GetLocalizedContentsSize(
        IDS_AUTOFILL_DIALOG_EDIT_CCARD_WIDTH_CHARS,
        IDS_AUTOFILL_DIALOG_EDIT_CCARD_HEIGHT_LINES);
  }
}

void AutoFillProfilesView::EditableSetViewContents::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  if (is_add && this == child) {
    observer_->ChildWindowOpened();
    views::GridLayout* layout = new views::GridLayout(this);
    layout->SetInsets(kSubViewVerticalInsets, kSubViewHorizotalInsets,
                      kSubViewVerticalInsets, kSubViewHorizotalInsets);
    SetLayoutManager(layout);
    InitLayoutGrid(layout);
    if (temporary_info_.is_address)
      InitAddressFields(layout);
    else
      InitCreditCardFields(layout);
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::DialogDelegate implementations
int AutoFillProfilesView::EditableSetViewContents::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL |
         MessageBoxFlags::DIALOGBUTTON_OK;
}

std::wstring
AutoFillProfilesView::EditableSetViewContents::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
  case MessageBoxFlags::DIALOGBUTTON_OK:
    return l10n_util::GetString(IDS_AUTOFILL_DIALOG_SAVE);
  case MessageBoxFlags::DIALOGBUTTON_CANCEL:
    return std::wstring();
  default:
    break;
  }
  NOTREACHED();
  return std::wstring();
}

bool AutoFillProfilesView::EditableSetViewContents::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
  case MessageBoxFlags::DIALOGBUTTON_OK: {
    bool phones_are_valid = true;
    for (std::vector<PhoneSubView*>::const_iterator it =
             phone_sub_views_.begin();
         it != phone_sub_views_.end() && phones_are_valid; ++it)
      phones_are_valid = (phones_are_valid && (*it)->IsValid());

    return phones_are_valid;
  }
  case MessageBoxFlags::DIALOGBUTTON_CANCEL:
    return true;
  default:
    break;
  }
  NOTREACHED();
  return false;
}

std::wstring
AutoFillProfilesView::EditableSetViewContents::GetWindowTitle() const {
  int string_id = 0;
  if (temporary_info_.is_address) {
    string_id = new_item_ ? IDS_AUTOFILL_ADD_ADDRESS_CAPTION :
                            IDS_AUTOFILL_EDIT_ADDRESS_CAPTION;
  } else {
    string_id = new_item_ ? IDS_AUTOFILL_ADD_CREDITCARD_CAPTION :
                            IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION;
  }
  return l10n_util::GetString(string_id);
}

void AutoFillProfilesView::EditableSetViewContents::WindowClosing() {
  billing_model_->ClearComboBoxes();
  observer_->ChildWindowClosed();
}

views::View* AutoFillProfilesView::EditableSetViewContents::GetContentsView() {
  return this;
}

bool AutoFillProfilesView::EditableSetViewContents::Cancel() {
  if (new_item_) {
    // Remove added item - it is last in the list.
    if (temporary_info_.is_address) {
      observer_->profiles_set_.pop_back();
    } else {
      observer_->credit_card_set_.pop_back();
    }
  }
  return true;
}

bool AutoFillProfilesView::EditableSetViewContents::Accept() {
  *editable_fields_set_ = temporary_info_;
  int index = -1;
  if (temporary_info_.is_address) {
    index = editable_fields_set_ - observer_->profiles_set_.begin();
  } else {
    index = editable_fields_set_ - observer_->credit_card_set_.begin();
    index += observer_->profiles_set_.size();
  }
  if (new_item_)
    observer_->table_model_->AddItem(index);
  else
    observer_->table_model_->UpdateItem(index);
  if (temporary_info_.is_address)
    observer_->UpdateProfileLabels();
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::ButtonListener implementations
void AutoFillProfilesView::EditableSetViewContents::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  NOTREACHED();
}


/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::Textfield::Controller implementations
void AutoFillProfilesView::EditableSetViewContents::ContentsChanged(
    views::Textfield* sender,  const string16& new_contents) {
  if (temporary_info_.is_address) {
    for (int field = 0; field < arraysize(address_fields_); ++field) {
      DCHECK(text_fields_[address_fields_[field].text_field]);
      if (text_fields_[address_fields_[field].text_field] == sender) {
        UpdateContentsPhoneViews(address_fields_[field].text_field,
                                 sender, new_contents);
        temporary_info_.address.SetInfo(
            AutoFillType(address_fields_[field].type), new_contents);
        UpdateButtons();
        return;
      }
    }
  } else {
    for (int field = 0; field < arraysize(credit_card_fields_); ++field) {
      DCHECK(text_fields_[credit_card_fields_[field].text_field]);
      if (text_fields_[credit_card_fields_[field].text_field] == sender) {
        UpdateContentsPhoneViews(address_fields_[field].text_field,
                                 sender, new_contents);
        temporary_info_.credit_card.SetInfo(
            AutoFillType(credit_card_fields_[field].type), new_contents);
        UpdateButtons();
        return;
      }
    }
  }
}

bool AutoFillProfilesView::EditableSetViewContents::HandleKeystroke(
    views::Textfield* sender, const views::Textfield::Keystroke& keystroke) {
  if (sender == text_fields_[TEXT_CC_NUMBER] &&
      !temporary_info_.has_credit_card_number_been_edited) {
    // You cannot edit obfuscated number, you must retype it anew.
    sender->SetText(string16());
    temporary_info_.has_credit_card_number_been_edited = true;
  }
  return false;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::Combobox::Listener implementations:
void AutoFillProfilesView::EditableSetViewContents::ItemChanged(
    views::Combobox* combo_box, int prev_index, int new_index) {
  DCHECK(billing_model_);
  if (combo_box == combo_box_billing_) {
    if (new_index == -1) {
      NOTREACHED();
    } else {
      DCHECK(new_index < static_cast<int>(observer_->profiles_set_.size()));
      temporary_info_.credit_card.set_billing_address(
          IntToString16(
          observer_->profiles_set_[new_index].address.unique_id()));
    }
  } else if (combo_box == combo_box_month_) {
    if (new_index == -1) {
      NOTREACHED();
    } else {
      temporary_info_.credit_card.SetInfo(
          AutoFillType(CREDIT_CARD_EXP_MONTH),
          combo_box_model_month_->GetItemAt(new_index));
    }
  } else if (combo_box == combo_box_year_) {
    if (new_index == -1) {
      NOTREACHED();
    } else {
      temporary_info_.credit_card.SetInfo(
          AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
          combo_box_model_year_->GetItemAt(new_index));
    }
  } else {
    NOTREACHED();
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, private:
void AutoFillProfilesView::EditableSetViewContents::InitAddressFields(
    views::GridLayout* layout) {
  DCHECK(temporary_info_.is_address);

  for (int field = 0; field < arraysize(address_fields_); ++field) {
    DCHECK(!text_fields_[address_fields_[field].text_field]);
    text_fields_[address_fields_[field].text_field] =
        new views::Textfield(views::Textfield::STYLE_DEFAULT);
    text_fields_[address_fields_[field].text_field]->SetController(this);
    text_fields_[address_fields_[field].text_field]->SetText(
        temporary_info_.address.GetFieldText(
        AutoFillType(address_fields_[field].type)));
  }

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_FULL_NAME));

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_FULL_NAME]);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_COMPANY_NAME));

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_COMPANY]);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_leading_view_set_id_);
  layout->AddView(new views::Label(l10n_util::GetString(
                  IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1)));

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_LINE_1]);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_leading_view_set_id_);
  layout->AddView(new views::Label(l10n_util::GetString(
                  IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2)));

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_LINE_2]);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_CITY));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_STATE));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_ZIP_CODE));

  text_fields_[TEXT_ADDRESS_CITY]->set_default_width_in_chars(16);
  text_fields_[TEXT_ADDRESS_STATE]->set_default_width_in_chars(16);
  text_fields_[TEXT_ADDRESS_ZIP]->set_default_width_in_chars(5);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_CITY]);
  layout->AddView(text_fields_[TEXT_ADDRESS_STATE]);
  layout->AddView(text_fields_[TEXT_ADDRESS_ZIP]);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_COUNTRY));

  text_fields_[TEXT_ADDRESS_COUNTRY]->set_default_width_in_chars(11);

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_COUNTRY]);

  PhoneSubView* phone = new PhoneSubView(
      observer_,
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_PHONE),
      text_fields_[TEXT_PHONE_PHONE]);

  phone_sub_views_.push_back(phone);

  PhoneSubView* fax = new PhoneSubView(
      observer_,
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_FAX),
      text_fields_[TEXT_FAX_PHONE]);

  phone_sub_views_.push_back(fax);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(phone);
  layout->AddView(fax);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_EMAIL));

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_EMAIL]);

  UpdateButtons();
}

void AutoFillProfilesView::EditableSetViewContents::InitCreditCardFields(
    views::GridLayout* layout) {
  DCHECK(!temporary_info_.is_address);
  DCHECK(billing_model_);

  // Create combo box models.
  combo_box_model_month_.reset(new StringVectorComboboxModel);
  std::vector<std::wstring> model_strings;
  model_strings.reserve(12);
  for (int month = 1; month <= 12; ++month)
    model_strings.push_back(StringPrintf(L"%02i", month));
  combo_box_model_month_->set_cb_strings(&model_strings);
  model_strings.clear();
  model_strings.reserve(20);
  base::Time::Exploded exploded_time;
  base::Time::Now().LocalExplode(&exploded_time);
  for (int year = 0; year < 10; ++year)
    model_strings.push_back(StringPrintf(L"%04i", year + exploded_time.year));
  combo_box_model_year_.reset(new StringVectorComboboxModel);
  combo_box_model_year_->set_cb_strings(&model_strings);

  for (int field = 0; field < arraysize(credit_card_fields_); ++field) {
    DCHECK(!text_fields_[credit_card_fields_[field].text_field]);
    text_fields_[credit_card_fields_[field].text_field] =
        new views::Textfield(views::Textfield::STYLE_DEFAULT);
    text_fields_[credit_card_fields_[field].text_field]->SetController(this);
    string16 field_text;
    if (credit_card_fields_[field].text_field == TEXT_CC_NUMBER) {
      field_text = temporary_info_.credit_card.GetFieldText(
          AutoFillType(credit_card_fields_[field].type));
      if (!field_text.empty())
        field_text = temporary_info_.credit_card.ObfuscatedNumber();
    } else {
      field_text = temporary_info_.credit_card.GetFieldText(
          AutoFillType(credit_card_fields_[field].type));
    }
    text_fields_[credit_card_fields_[field].text_field]->SetText(field_text);
  }

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_NAME_ON_CARD));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_CC_NAME]);

  // Address combo boxes.
  combo_box_billing_ = new views::Combobox(billing_model_);
  combo_box_billing_->set_listener(this);
  int billing_id = -1;
  if (StringToInt(temporary_info_.credit_card.billing_address(), &billing_id))
    combo_box_billing_->SetSelectedItem(billing_model_->GetIndex(billing_id));
  billing_model_->UsedWithComboBox(combo_box_billing_);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_BILLING_ADDRESS));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(combo_box_billing_);

  // Layout credit card info
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_ccnumber_cvc_);
  layout->AddView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER));
  layout->StartRow(0, double_column_ccnumber_cvc_);
  // Number (20 chars), month(2 chars), year (4 chars), cvc (4 chars)
  text_fields_[TEXT_CC_NUMBER]->set_default_width_in_chars(20);
  layout->AddView(text_fields_[TEXT_CC_NUMBER]);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_ccexpiration_);
  layout->AddView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_EXPIRATION_DATE), 3, 1);

  combo_box_month_ = new views::Combobox(combo_box_model_month_.get());
  combo_box_month_->set_listener(this);
  string16 field_text;
  field_text = temporary_info_.credit_card.GetFieldText(
       AutoFillType(CREDIT_CARD_EXP_MONTH));
  combo_box_month_->SetSelectedItem(
      combo_box_model_month_->GetIndex(field_text));

  combo_box_year_ = new views::Combobox(combo_box_model_year_.get());
  combo_box_year_->set_listener(this);
  field_text = temporary_info_.credit_card.GetFieldText(
      AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  combo_box_year_->SetSelectedItem(combo_box_model_year_->GetIndex(field_text));

  layout->StartRow(0, double_column_ccexpiration_);
  layout->AddView(combo_box_month_);
  layout->AddView(combo_box_year_);

  UpdateButtons();
}

void AutoFillProfilesView::EditableSetViewContents::InitLayoutGrid(
    views::GridLayout* layout) {
  views::ColumnSet* column_set =
      layout->AddColumnSet(double_column_fill_view_set_id_);
  int i;
  for (i = 0; i < 2; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                          views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(double_column_leading_view_set_id_);
  for (i = 0; i < 2; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1, views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(triple_column_fill_view_set_id_);
  for (i = 0; i < 3; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                          views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(triple_column_leading_view_set_id_);
  for (i = 0; i < 3; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1, views::GridLayout::USE_PREF, 0, 0);
  }
  // City (33% - 16/48), state(33%), zip (12.7% - 5/42), country (21% - 11/48)
  column_set = layout->AddColumnSet(four_column_city_state_zip_set_id_);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        16, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        16, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        5, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        11, views::GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(double_column_ccnumber_cvc_);
  // Number and CVC are in ratio 20:4
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        20, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        4, views::GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(double_column_ccexpiration_);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);

  // The sizes: 4 characters for drop down icon + 2 for a month or 4 for a year.
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        0, views::GridLayout::FIXED,
                        font.GetStringWidth(std::wstring(L"000000")), 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        0, views::GridLayout::FIXED,
                        font.GetStringWidth(std::wstring(L"00000000")), 0);

  column_set = layout->AddColumnSet(three_column_header_);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        1, views::GridLayout::FIXED, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        1, views::GridLayout::FIXED, 0, 0);
}

views::Label*
AutoFillProfilesView::EditableSetViewContents::CreateLeftAlignedLabel(
    int label_id) {
  views::Label* label = new views::Label(l10n_util::GetString(label_id));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

void AutoFillProfilesView::EditableSetViewContents::UpdateButtons() {
  GetDialogClientView()->UpdateDialogButtons();
}

void AutoFillProfilesView::EditableSetViewContents::UpdateContentsPhoneViews(
    TextFields field, views::Textfield* sender, const string16& new_contents) {
  switch (field) {
    case TEXT_PHONE_PHONE:
    case TEXT_FAX_PHONE: {
      for (std::vector<PhoneSubView*>::iterator it = phone_sub_views_.begin();
           it != phone_sub_views_.end(); ++it)
        (*it)->ContentsChanged(sender, new_contents);
    } break;
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AddressComboBoxModel, public:
AutoFillProfilesView::AddressComboBoxModel::AddressComboBoxModel(
    bool is_billing)
    : address_labels_(NULL),
      is_billing_(is_billing) {
}

void AutoFillProfilesView::AddressComboBoxModel::set_address_labels(
    const std::vector<EditableSetInfo>* address_labels) {
  DCHECK(!address_labels_);
  address_labels_ = address_labels;
}

void AutoFillProfilesView::AddressComboBoxModel::UsedWithComboBox(
    views::Combobox* combo_box) {
  DCHECK(address_labels_);
  combo_boxes_.push_back(combo_box);
}

void AutoFillProfilesView::AddressComboBoxModel::LabelChanged() {
  DCHECK(address_labels_);
  for (std::list<views::Combobox*>::iterator it = combo_boxes_.begin();
       it != combo_boxes_.end();
       ++it)
    (*it)->ModelChanged();
}

int AutoFillProfilesView::AddressComboBoxModel::GetIndex(int unique_id) {
  int shift = is_billing_ ? 0 : 1;
  DCHECK(address_labels_);
  for (size_t i = 0; i < address_labels_->size(); ++i) {
    DCHECK(address_labels_->at(i).is_address);
    if (address_labels_->at(i).address.unique_id() == unique_id)
      return i + shift;
  }
  return -1;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AddressComboBoxModel,  ComboboxModel methods
int AutoFillProfilesView::AddressComboBoxModel::GetItemCount() {
  DCHECK(address_labels_);
  int shift = is_billing_ ? 0 : 1;
  return static_cast<int>(address_labels_->size()) + shift;
}

std::wstring AutoFillProfilesView::AddressComboBoxModel::GetItemAt(int index) {
  DCHECK(address_labels_);
  int shift = is_billing_ ? 0 : 1;
  DCHECK(index < (static_cast<int>(address_labels_->size()) + shift));
  if (!is_billing_ && !index)
    return l10n_util::GetString(IDS_AUTOFILL_DIALOG_SAME_AS_BILLING);
  DCHECK(address_labels_->at(index - shift).is_address);
  std::wstring label =
      address_labels_->at(index - shift).address.Label();
  if (label.empty())
    label = l10n_util::GetString(IDS_AUTOFILL_NEW_ADDRESS);
  return label;
}

void AutoFillProfilesView::StringVectorComboboxModel::set_cb_strings(
    std::vector<std::wstring> *source) {
  cb_strings_.swap(*source);
}

int AutoFillProfilesView::StringVectorComboboxModel::GetItemCount() {
  return cb_strings_.size();
}

std::wstring AutoFillProfilesView::StringVectorComboboxModel::GetItemAt(
    int index) {
  DCHECK_GT(static_cast<int>(cb_strings_.size()), index);
  return cb_strings_[index];
}

int AutoFillProfilesView::StringVectorComboboxModel::GetIndex(
    const std::wstring& value) {
  for (size_t index = 0; index < cb_strings_.size(); ++index) {
    if (cb_strings_[index] == value)
      return index;
  }
  return -1;
}

AutoFillProfilesView::ContentListTableModel::ContentListTableModel(
    std::vector<EditableSetInfo>* profiles,
    std::vector<EditableSetInfo>* credit_cards)
    : profiles_(profiles),
      credit_cards_(credit_cards) {
}

void AutoFillProfilesView::ContentListTableModel::Refresh() {
  if (observer_)
    observer_->OnModelChanged();
}

void AutoFillProfilesView::ContentListTableModel::AddItem(int index) {
  if (observer_)
    observer_->OnItemsAdded(index, 1);
}

void AutoFillProfilesView::ContentListTableModel::RemoveItem(int index) {
  DCHECK(index < static_cast<int>(profiles_->size() + credit_cards_->size()));
  if (index < static_cast<int>(profiles_->size()))
    profiles_->erase(profiles_->begin() + index);
  else
    credit_cards_->erase(credit_cards_->begin() + (index - profiles_->size()));
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

void AutoFillProfilesView::ContentListTableModel::UpdateItem(int index) {
  if (observer_)
    observer_->OnItemsChanged(index, 1);
}

int AutoFillProfilesView::ContentListTableModel::RowCount() {
  return profiles_->size() + credit_cards_->size();
}

std::wstring AutoFillProfilesView::ContentListTableModel::GetText(
    int row, int column_id) {
  DCHECK(row < static_cast<int>(profiles_->size() + credit_cards_->size()));
  if (row < static_cast<int>(profiles_->size())) {
    return profiles_->at(row).address.PreviewSummary();
  } else {
    row -= profiles_->size();
    return credit_cards_->at(row).credit_card.PreviewSummary();
  }
}

TableModel::Groups AutoFillProfilesView::ContentListTableModel::GetGroups() {
  TableModel::Groups groups;

  TableModel::Group profile_group;
  profile_group.title = l10n_util::GetString(IDS_AUTOFILL_ADDRESSES_GROUP_NAME);
  profile_group.id = kAddressGroup;
  groups.push_back(profile_group);

  Group cc_group;
  cc_group.title = l10n_util::GetString(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME);
  cc_group.id = kCreditCardGroup;
  groups.push_back(cc_group);

  return groups;
}

int AutoFillProfilesView::ContentListTableModel::GetGroupID(int row) {
  DCHECK(row < static_cast<int>(profiles_->size() + credit_cards_->size()));
  return (row < static_cast<int>(profiles_->size())) ? kAddressGroup :
                                                       kCreditCardGroup;
}

void AutoFillProfilesView::ContentListTableModel::SetObserver(
    TableModelObserver* observer) {
  observer_ = observer;
}


// Declared in "chrome/browser/autofill/autofill_dialog.h"
void ShowAutoFillDialog(gfx::NativeView parent,
                        AutoFillDialogObserver* observer,
                        Profile* profile) {
  DCHECK(profile);

  PersonalDataManager* personal_data_manager =
      profile->GetPersonalDataManager();
  DCHECK(personal_data_manager);
  AutoFillProfilesView::Show(parent, observer, personal_data_manager, profile,
                             profile->GetPrefs(), NULL, NULL);
}
