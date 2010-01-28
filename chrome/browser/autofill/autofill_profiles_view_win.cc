// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/autofill/autofill_profiles_view_win.h"

#include <vsstyle.h>
#include <vssym32.h>

#include "app/gfx/canvas.h"
#include "app/gfx/native_theme_win.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/size.h"
#include "base/message_loop.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/scroll_view.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace {

// padding on the sides of AutoFill settings dialog.
const int kDialogPadding = 7;

// Insets for subview controls.
const int kSubViewInsets = 5;

// TODO(georgey) remove this code into a separate file as it is already the same
// elsewhere.
// A background object that paints the scrollable list background,
// which may be rendered by the system visual styles system.
class ListBackground : public views::Background {
 public:
  explicit ListBackground() {
    SkColor list_color =
        gfx::NativeTheme::instance()->GetThemeColorWithDefault(
        gfx::NativeTheme::LIST, 1, TS_NORMAL, TMT_FILLCOLOR, COLOR_WINDOW);
    SetNativeControlColor(list_color);
  }
  virtual ~ListBackground() {}

  virtual void Paint(gfx::Canvas* canvas, views::View* view) const {
    HDC dc = canvas->beginPlatformPaint();
    RECT native_lb = view->GetLocalBounds(true).ToRECT();
    gfx::NativeTheme::instance()->PaintListBackground(dc, true, &native_lb);
    canvas->endPlatformPaint();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ListBackground);
};

};  // namespace

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, static data:
AutoFillProfilesView *AutoFillProfilesView::instance_ = NULL;

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, static data:
int AutoFillProfilesView::ScrollViewContents::line_height_ = 0;

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, public:
AutoFillProfilesView::AutoFillProfilesView(
    AutoFillDialogObserver* observer,
    const std::vector<AutoFillProfile*>& profiles,
    const std::vector<CreditCard*>& credit_cards)
    : observer_(observer),
      scroll_view_(NULL),
      save_changes_(NULL) {
  profiles_set_.reserve(profiles.size());
  for (std::vector<AutoFillProfile*>::const_iterator address_it =
           profiles.begin();
       address_it != profiles.end();
       ++address_it) {
    profiles_set_.push_back(EditableSetInfo(*address_it, true));
  }
  credit_card_set_.reserve(credit_cards.size());
  for (std::vector<CreditCard*>::const_iterator cc_it = credit_cards.begin();
       cc_it != credit_cards.end();
       ++cc_it) {
    credit_card_set_.push_back(EditableSetInfo(*cc_it, true));
  }
}

AutoFillProfilesView::~AutoFillProfilesView() {
}

int AutoFillProfilesView::Show(AutoFillDialogObserver* observer,
                               const std::vector<AutoFillProfile*>& profiles,
                               const std::vector<CreditCard*>& credit_cards) {
  if (!instance_) {
    instance_ = new AutoFillProfilesView(observer, profiles, credit_cards);

    // |instance_| will get deleted once Close() is called.
    views::Window::CreateChromeWindow(NULL, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible())
    instance_->window()->Show();
  else
    instance_->window()->Activate();
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, protected:
/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::View implementations
void AutoFillProfilesView::Layout() {
  scroll_view_->SetBounds(kDialogPadding, kDialogPadding,
                          width() - (2 * kDialogPadding),
                          height() - (2 * kDialogPadding));
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
  return MessageBoxFlags::DIALOGBUTTON_CANCEL |
         MessageBoxFlags::DIALOGBUTTON_OK;
}

std::wstring AutoFillProfilesView::GetDialogButtonLabel(
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

std::wstring AutoFillProfilesView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_AUTOFILL_DIALOG_TITLE);
}

void AutoFillProfilesView::WindowClosing() {
  instance_ = NULL;
}

views::View* AutoFillProfilesView::GetContentsView() {
  return this;
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
  observer_->OnAutoFillDialogApply(&profiles, &credit_cards);
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, views::ButtonListener implementations:
void AutoFillProfilesView::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
  NOTIMPLEMENTED();
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView, private:
void AutoFillProfilesView::Init() {
  scroll_view_ = new AutoFillScrollView(&profiles_set_, &credit_card_set_);

  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
    views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, single_column_view_set_id);
  layout->AddView(scroll_view_);
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::PhoneSubView, public:
AutoFillProfilesView::PhoneSubView::PhoneSubView(
    views::Label* label,
    views::Textfield* text_country,
    views::Textfield* text_area,
    views::Textfield* text_phone)
    : label_(label),
      text_country_(text_country),
      text_area_(text_area),
      text_phone_(text_phone) {
  DCHECK(label_);
  DCHECK(text_country_);
  DCHECK(text_area_);
  DCHECK(text_phone_);
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
    const int triple_column_fill_view_set_id = 0;
    views::ColumnSet* column_set =
        layout->AddColumnSet(triple_column_fill_view_set_id);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
        views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
        views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
         views::GridLayout::USE_PREF, 0, 0);
    layout->StartRow(0, triple_column_fill_view_set_id);
    layout->AddView(label_, 5, 1);
    layout->StartRow(0, triple_column_fill_view_set_id);
    text_country_->set_default_width_in_chars(3);
    text_area_->set_default_width_in_chars(3);
    text_phone_->set_default_width_in_chars(7);
    layout->AddView(text_country_);
    layout->AddView(text_area_);
    layout->AddView(text_phone_);
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, static data:
AutoFillProfilesView::EditableSetViewContents::TextFieldToAutoFill
    AutoFillProfilesView::EditableSetViewContents::address_fields_[] = {
  { AutoFillProfilesView::EditableSetViewContents::TEXT_LABEL, NO_SERVER_DATA },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FIRST_NAME,
    NAME_FIRST },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_MIDDLE_NAME,
    NAME_MIDDLE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_LAST_NAME,
    NAME_LAST },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_EMAIL, EMAIL_ADDRESS },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_COMPANY_NAME,
    COMPANY_NAME },
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
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_COUNTRY,
    PHONE_HOME_COUNTRY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_AREA,
    PHONE_HOME_CITY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_PHONE,
    PHONE_HOME_NUMBER },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FAX_COUNTRY,
    PHONE_FAX_COUNTRY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FAX_AREA,
    PHONE_FAX_CITY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_FAX_PHONE,
    PHONE_FAX_NUMBER },
};

AutoFillProfilesView::EditableSetViewContents::TextFieldToAutoFill
    AutoFillProfilesView::EditableSetViewContents::credit_card_fields_[] = {
  { AutoFillProfilesView::EditableSetViewContents::TEXT_LABEL, NO_SERVER_DATA },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_NAME,
    CREDIT_CARD_NAME },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_NUMBER,
    CREDIT_CARD_NUMBER },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_EXPIRATION_MONTH,
    CREDIT_CARD_EXP_MONTH },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_EXPIRATION_YEAR,
    CREDIT_CARD_EXP_2_DIGIT_YEAR },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_CC_EXPIRATION_CVC,
    CREDIT_CARD_VERIFICATION_CODE },
  /* phone is disabled for now
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_COUNTRY,
  PHONE_HOME_COUNTRY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_AREA,
  PHONE_HOME_CITY_CODE },
  { AutoFillProfilesView::EditableSetViewContents::TEXT_PHONE_PHONE,
  PHONE_HOME_NUMBER },
  */
};

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, public:
AutoFillProfilesView::EditableSetViewContents::EditableSetViewContents(
    std::vector<EditableSetInfo>::iterator field_set)
    : editable_fields_set_(field_set),
      delete_button_(NULL),
      title_label_(NULL) {
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
  gfx::Size prefsize;
  views::View* parent = GetParent();
  if (parent && parent->width()) {
    const int width = parent->width();
    prefsize = gfx::Size(width, GetHeightForWidth(width));
  }
  return prefsize;
}

void AutoFillProfilesView::EditableSetViewContents::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  if (is_add && this == child) {
    views::GridLayout* layout = new views::GridLayout(this);
    layout->SetInsets(kSubViewInsets, kSubViewInsets,
                      kSubViewInsets, kSubViewInsets);
    SetLayoutManager(layout);
    InitLayoutGrid(layout);
    delete_button_ = new views::NativeButton(this,
        l10n_util::GetString(IDS_AUTOFILL_DELETE_BUTTON));
    if (editable_fields_set_->is_address)
      InitAddressFields(layout);
    else
      InitCreditCardFields(layout);
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::Textfield::Controller implementations
void AutoFillProfilesView::EditableSetViewContents::ContentsChanged(
    views::Textfield* sender,  const string16& new_contents) {
  if (editable_fields_set_->is_address) {
    for (int field = 0; field < arraysize(address_fields_); ++field) {
      DCHECK(text_fields_[address_fields_[field].text_field]);
      if (text_fields_[address_fields_[field].text_field] == sender) {
        if (address_fields_[field].text_field == TEXT_LABEL)
          editable_fields_set_->address.set_label(new_contents);
        else
          editable_fields_set_->address.SetInfo(
              AutoFillType(address_fields_[field].type), new_contents);
        return;
      }
    }
  } else {
    for (int field = 0; field < arraysize(credit_card_fields_); ++field) {
      DCHECK(text_fields_[credit_card_fields_[field].text_field]);
      if (text_fields_[credit_card_fields_[field].text_field] == sender) {
        if (credit_card_fields_[field].text_field == TEXT_LABEL)
          editable_fields_set_->credit_card.set_label(new_contents);
        else
          editable_fields_set_->credit_card.SetInfo(
              AutoFillType(credit_card_fields_[field].type), new_contents);
        return;
      }
    }
  }
}

bool AutoFillProfilesView::EditableSetViewContents::HandleKeystroke(
    views::Textfield* sender, const views::Textfield::Keystroke& keystroke) {
  return false;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::ButtonListener implementations
void AutoFillProfilesView::EditableSetViewContents::ButtonPressed(
  views::Button* sender, const views::Event& event) {
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, private:
void AutoFillProfilesView::EditableSetViewContents::InitAddressFields(
    views::GridLayout* layout) {
  DCHECK(editable_fields_set_->is_address);
  std::wstring title = editable_fields_set_->address.Label();
  if (title.empty())
    title = l10n_util::GetString(IDS_AUTOFILL_NEW_ADDRESS);
  title_label_ = new views::Label(title);

  for (int field = 0; field < arraysize(address_fields_); ++field) {
    DCHECK(!text_fields_[address_fields_[field].text_field]);
    text_fields_[address_fields_[field].text_field] =
        new views::Textfield(views::Textfield::STYLE_DEFAULT);
    text_fields_[address_fields_[field].text_field]->SetController(this);
    if (address_fields_[field].text_field == TEXT_LABEL) {
      text_fields_[TEXT_LABEL]->SetText(
          editable_fields_set_->address.Label());
    } else {
      text_fields_[address_fields_[field].text_field]->SetText(
          editable_fields_set_->address.GetFieldText(
          AutoFillType(address_fields_[field].type)));
    }
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label_->SetFont(title_font);

  SkColor title_color =
      gfx::NativeTheme::instance()->GetThemeColorWithDefault(
      gfx::NativeTheme::BUTTON, BP_GROUPBOX, GBS_NORMAL, TMT_TEXTCOLOR,
      COLOR_WINDOWTEXT);
  title_label_->SetColor(title_color);
  SkColor bk_color =
       gfx::NativeTheme::instance()->GetThemeColorWithDefault(
       gfx::NativeTheme::BUTTON, BP_PUSHBUTTON, PBS_NORMAL, TMT_BTNFACE,
       COLOR_BTNFACE);
  title_label_->set_background(
      views::Background::CreateSolidBackground(bk_color));
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(title_label_, 5, 1);
  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_AUTOFILL_DIALOG_LABEL)));
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_LABEL]);
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_FIRST_NAME));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_MIDDLE_NAME));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_LAST_NAME));
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_FIRST_NAME]);
  layout->AddView(text_fields_[TEXT_MIDDLE_NAME]);
  layout->AddView(text_fields_[TEXT_LAST_NAME]);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_EMAIL));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_COMPANY_NAME));

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_EMAIL]);
  layout->AddView(text_fields_[TEXT_COMPANY_NAME]);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(new views::Label(l10n_util::GetString(
                  IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1)), 3, 1);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_LINE_1], 3, 1);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(new views::Label(l10n_util::GetString(
                  IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2)), 3, 1);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_LINE_2], 3, 1);

  layout->StartRow(0, four_column_city_state_zip_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_CITY));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_STATE));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_ZIP_CODE));
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_COUNTRY));
  // City (33% - 16/48), state(33%), zip (12.7% - 5/42), country (21% - 11/48)
  text_fields_[TEXT_ADDRESS_CITY]->set_default_width_in_chars(16);
  text_fields_[TEXT_ADDRESS_STATE]->set_default_width_in_chars(16);
  text_fields_[TEXT_ADDRESS_ZIP]->set_default_width_in_chars(5);
  text_fields_[TEXT_ADDRESS_COUNTRY]->set_default_width_in_chars(11);

  layout->StartRow(0, four_column_city_state_zip_set_id_);
  layout->AddView(text_fields_[TEXT_ADDRESS_CITY]);
  layout->AddView(text_fields_[TEXT_ADDRESS_STATE]);
  layout->AddView(text_fields_[TEXT_ADDRESS_ZIP]);
  layout->AddView(text_fields_[TEXT_ADDRESS_COUNTRY]);

  PhoneSubView *phone = new PhoneSubView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_PHONE),
      text_fields_[TEXT_PHONE_COUNTRY],
      text_fields_[TEXT_PHONE_AREA],
      text_fields_[TEXT_PHONE_PHONE]);

  PhoneSubView *fax = new PhoneSubView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_FAX),
      text_fields_[TEXT_FAX_COUNTRY],
      text_fields_[TEXT_FAX_AREA],
      text_fields_[TEXT_FAX_PHONE]);

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(phone);
  layout->AddView(fax);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(delete_button_);
}

void AutoFillProfilesView::EditableSetViewContents::InitCreditCardFields(
    views::GridLayout* layout) {
  DCHECK(!editable_fields_set_->is_address);
  std::wstring title = editable_fields_set_->credit_card.Label();
  if (title.empty())
     title = l10n_util::GetString(IDS_AUTOFILL_NEW_CREDITCARD);
  title_label_ = new views::Label(title);

  for (int field = 0; field < arraysize(credit_card_fields_); ++field) {
    DCHECK(!text_fields_[credit_card_fields_[field].text_field]);
    text_fields_[credit_card_fields_[field].text_field] =
        new views::Textfield(views::Textfield::STYLE_DEFAULT);
    text_fields_[credit_card_fields_[field].text_field]->SetController(this);
    if (credit_card_fields_[field].text_field == TEXT_LABEL) {
      text_fields_[TEXT_LABEL]->SetText(
          editable_fields_set_->credit_card.Label());
    } else {
      text_fields_[credit_card_fields_[field].text_field]->SetText(
          editable_fields_set_->credit_card.GetFieldText(
          AutoFillType(credit_card_fields_[field].type)));
    }
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label_->SetFont(title_font);

  SkColor title_color =
      gfx::NativeTheme::instance()->GetThemeColorWithDefault(
      gfx::NativeTheme::BUTTON, BP_GROUPBOX, GBS_NORMAL, TMT_TEXTCOLOR,
      COLOR_WINDOWTEXT);
  title_label_->SetColor(title_color);
  SkColor bk_color =
      gfx::NativeTheme::instance()->GetThemeColorWithDefault(
      gfx::NativeTheme::BUTTON, BP_PUSHBUTTON, PBS_NORMAL, TMT_BTNFACE,
      COLOR_BTNFACE);
  title_label_->set_background(
      views::Background::CreateSolidBackground(bk_color));
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(title_label_, 5, 1);

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_LABEL));
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_LABEL]);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_NAME_ON_CARD));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_CC_NAME]);
  layout->StartRow(0, four_column_ccnumber_expiration_cvc_);
  layout->AddView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER));
  layout->AddView(
      CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_EXPIRATION_DATE), 2, 1);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_CVC));
  layout->StartRow(0, four_column_ccnumber_expiration_cvc_);
  // Number (20 chars), month(2 chars), year (4 chars), cvc (4 chars)
  text_fields_[TEXT_CC_NUMBER]->set_default_width_in_chars(20);
  text_fields_[TEXT_CC_EXPIRATION_MONTH]->set_default_width_in_chars(2);
  text_fields_[TEXT_CC_EXPIRATION_YEAR]->set_default_width_in_chars(4);
  text_fields_[TEXT_CC_EXPIRATION_CVC]->set_default_width_in_chars(4);
  layout->AddView(text_fields_[TEXT_CC_NUMBER]);
  layout->AddView(text_fields_[TEXT_CC_EXPIRATION_MONTH]);
  layout->AddView(text_fields_[TEXT_CC_EXPIRATION_YEAR]);
  layout->AddView(text_fields_[TEXT_CC_EXPIRATION_CVC]);
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
  column_set = layout->AddColumnSet(four_column_city_state_zip_set_id_);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(four_column_ccnumber_expiration_cvc_);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
}

views::Label*
AutoFillProfilesView::EditableSetViewContents::CreateLeftAlignedLabel(
    int label_id) {
  views::Label* label = new views::Label(l10n_util::GetString(label_id));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, public:
AutoFillProfilesView::ScrollViewContents::ScrollViewContents(
    std::vector<EditableSetInfo>* profiles,
    std::vector<EditableSetInfo>* credit_cards)
    : profiles_(profiles),
      credit_cards_(credit_cards),
      add_address_(NULL),
      add_credit_card_(NULL) {
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, protected:
/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, views::View implementations
int AutoFillProfilesView::ScrollViewContents::GetLineScrollIncrement(
    views::ScrollView* scroll_view, bool is_horizontal, bool is_positive) {
  if (!is_horizontal)
    return line_height_;
  return View::GetPageScrollIncrement(scroll_view, is_horizontal, is_positive);
}

void AutoFillProfilesView::ScrollViewContents::Layout() {
  views::View* parent = GetParent();
  if (parent && parent->width()) {
    const int width = parent->width();
    const int height = GetHeightForWidth(width);
    SetBounds(0, 0, width, height);
  } else {
    gfx::Size prefsize = GetPreferredSize();
    SetBounds(0, 0, prefsize.width(), prefsize.height());
  }
  View::Layout();
}

gfx::Size AutoFillProfilesView::ScrollViewContents::GetPreferredSize() {
  return gfx::Size();
}

void AutoFillProfilesView::ScrollViewContents::ViewHierarchyChanged(
     bool is_add, views::View* parent, views::View* child) {
  if (is_add && this == child) {
    if (!line_height_) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      line_height_ = rb.GetFont(ResourceBundle::BaseFont).height();
    }

    gfx::Rect lb = GetLocalBounds(false);
    SetBounds(lb);

    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);

    const int single_column_filled_view_set_id = 0;
    views::ColumnSet* column_set =
        layout->AddColumnSet(single_column_filled_view_set_id);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::USE_PREF, 0, 0);
    const int single_column_left_view_set_id = 1;
    column_set = layout->AddColumnSet(single_column_left_view_set_id);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                          1, views::GridLayout::USE_PREF, 0, 0);
    views::Label *title_label = new views::Label(
        l10n_util::GetString(IDS_AUTOFILL_ADDRESSES_GROUP_NAME));
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    gfx::Font title_font =
        rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
    title_label->SetFont(title_font);
    layout->StartRow(0, single_column_left_view_set_id);
    layout->AddView(title_label);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    std::vector<EditableSetInfo>::iterator it;
    for (it = profiles_->begin(); it != profiles_->end(); ++it) {
      EditableSetViewContents *address_view =
          new EditableSetViewContents(it);
      layout->StartRow(0, single_column_filled_view_set_id);
      layout->AddView(address_view);
      layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    }

    add_address_ = new views::NativeButton(this,
        l10n_util::GetString(IDS_AUTOFILL_ADD_ADDRESS_BUTTON));
    layout->StartRow(0, single_column_left_view_set_id);
    layout->AddView(add_address_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    title_label = new views::Label(
        l10n_util::GetString(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME));
    title_label->SetFont(title_font);
    layout->StartRow(0, single_column_left_view_set_id);
    layout->AddView(title_label);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    for (it = credit_cards_->begin(); it != credit_cards_->end(); ++it) {
      EditableSetViewContents *address_view =
          new EditableSetViewContents(it);
      layout->StartRow(0, single_column_filled_view_set_id);
      layout->AddView(address_view);
      layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    }

    add_credit_card_ = new views::NativeButton(this,
        l10n_util::GetString(IDS_AUTOFILL_ADD_CREDITCARD_BUTTON));

    layout->StartRow(0, single_column_left_view_set_id);
    layout->AddView(add_credit_card_);
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents,
// views::ButtonListener implementations
void AutoFillProfilesView::ScrollViewContents::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  NOTIMPLEMENTED();
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AutoFillScrollView, public:
AutoFillProfilesView::AutoFillScrollView::AutoFillScrollView(
    std::vector<EditableSetInfo>* profiles,
    std::vector<EditableSetInfo>* credit_cards)
    : scroll_view_(new views::ScrollView),
      scroll_contents_view_(new ScrollViewContents(profiles, credit_cards)) {
  AddChildView(scroll_view_);
  scroll_view_->SetContents(scroll_contents_view_);
  set_background(new ListBackground());
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AutoFillScrollView, views::View implementations
void AutoFillProfilesView::AutoFillScrollView::Layout() {
  gfx::Rect lb = GetLocalBounds(false);

  gfx::Size border = gfx::NativeTheme::instance()->GetThemeBorderSize(
      gfx::NativeTheme::LIST);
  lb.Inset(border.width(), border.height());
  scroll_view_->SetBounds(lb);
  scroll_view_->Layout();
}

