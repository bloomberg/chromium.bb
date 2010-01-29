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
// TODO(georgey) remove this when resources available.
#include "chrome/browser/theme_resources_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/image_button.h"
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
void AutoFillProfilesView::AddClicked(EditableSetType item_type) {
  if (item_type == EDITABLE_SET_ADDRESS) {
    AutoFillProfile address(std::wstring(), 0);
    profiles_set_.push_back(EditableSetInfo(&address, true));
  } else if (item_type == EDITABLE_SET_CREDIT_CARD) {
    CreditCard credit_card(std::wstring(), 0);
    credit_card_set_.push_back(EditableSetInfo(&credit_card, true));
  } else {
    NOTREACHED();
  }
  scroll_view_->RebuildView();
}

void AutoFillProfilesView::DeleteEditableSet(
    std::vector<EditableSetInfo>::iterator field_set_iterator) {
  if (field_set_iterator->is_address) {
    string16 label = field_set_iterator->address.Label();
    profiles_set_.erase(field_set_iterator);
    for (std::vector<EditableSetInfo>::iterator it = credit_card_set_.begin();
         it != credit_card_set_.end();
         ++it) {
      if (it->credit_card.shipping_address() == label)
        it->credit_card.set_shipping_address(string16());
      if (it->credit_card.billing_address() == label)
        it->credit_card.set_billing_address(string16());
    }
  } else {
    credit_card_set_.erase(field_set_iterator);
  }
  scroll_view_->RebuildView();
}

void AutoFillProfilesView::CollapseStateChanged(
    std::vector<EditableSetInfo>::iterator field_set_iterator) {
  scroll_view_->RebuildView();
}


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
  scroll_view_ = new AutoFillScrollView(this,
                                        &profiles_set_,
                                        &credit_card_set_);

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
    AutoFillProfilesView* observer,
    AddressComboBoxModel* billing_model,
    AddressComboBoxModel* shipping_model,
    std::vector<EditableSetInfo>::iterator field_set)
    : editable_fields_set_(field_set),
      delete_button_(NULL),
      expand_item_button_(NULL),
      title_label_(NULL),
      title_label_preview_(NULL),
      observer_(observer),
      billing_model_(billing_model),
      shipping_model_(shipping_model),
      combo_box_billing_(NULL),
      combo_box_shipping_(NULL) {
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

    InitTitle(layout);
    if (editable_fields_set_->is_opened) {
      if (editable_fields_set_->is_address)
        InitAddressFields(layout);
      else
        InitCreditCardFields(layout);
    }
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
        if (address_fields_[field].text_field == TEXT_LABEL) {
          editable_fields_set_->address.set_label(new_contents);
          title_label_->SetText(new_contents);
          // One of the address labels changed - update combo boxes
          billing_model_->LabelChanged();
          shipping_model_->LabelChanged();
        } else {
          editable_fields_set_->address.SetInfo(
              AutoFillType(address_fields_[field].type), new_contents);
        }
        return;
      }
    }
  } else {
    for (int field = 0; field < arraysize(credit_card_fields_); ++field) {
      DCHECK(text_fields_[credit_card_fields_[field].text_field]);
      if (text_fields_[credit_card_fields_[field].text_field] == sender) {
        if (credit_card_fields_[field].text_field == TEXT_LABEL) {
          editable_fields_set_->credit_card.set_label(new_contents);
          title_label_->SetText(new_contents);
        } else {
          editable_fields_set_->credit_card.SetInfo(
              AutoFillType(credit_card_fields_[field].type), new_contents);
        }
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
// views::ButtonListener implementations:
void AutoFillProfilesView::EditableSetViewContents::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == delete_button_) {
    observer_->DeleteEditableSet(editable_fields_set_);
  } else if (sender == expand_item_button_) {
    editable_fields_set_->is_opened = !editable_fields_set_->is_opened;
    observer_->CollapseStateChanged(editable_fields_set_);
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents,
// views::Combobox::Listener implementations:
void AutoFillProfilesView::EditableSetViewContents::ItemChanged(
    views::Combobox* combo_box, int prev_index, int new_index) {
  DCHECK(billing_model_);
  DCHECK(shipping_model_);
  if (combo_box == combo_box_billing_) {
    if (new_index == -1) {
      NOTREACHED();
    } else {
      editable_fields_set_->credit_card.set_billing_address(
          billing_model_->GetItemAt(new_index));
    }
  } else if (combo_box == combo_box_shipping_) {
    if (new_index == -1) {
      NOTREACHED();
    } else if (new_index == 0) {
      editable_fields_set_->credit_card.set_shipping_address(
          editable_fields_set_->credit_card.billing_address());
    } else {
      editable_fields_set_->credit_card.set_shipping_address(
          shipping_model_->GetItemAt(new_index));
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::EditableSetViewContents, private:
void AutoFillProfilesView::EditableSetViewContents::InitTitle(
    views::GridLayout* layout) {
  std::wstring title;
  std::wstring title_preview;
  if (editable_fields_set_->is_address) {
    title = editable_fields_set_->address.Label();
    if (title.empty())
      title = l10n_util::GetString(IDS_AUTOFILL_NEW_ADDRESS);
    // TODO(georgey): build address preview  correctly
    title_preview = editable_fields_set_->address.GetFieldText(
        AutoFillType(NAME_FIRST));
    title_preview.append(L" ");
    std::wstring middle = editable_fields_set_->address.GetFieldText(
        AutoFillType(NAME_MIDDLE));
    if (!middle.empty()) {
      title_preview.append(middle);
      title_preview.append(L" ");
    }
    title_preview.append(editable_fields_set_->address.GetFieldText(
        AutoFillType(NAME_LAST)));
    title_preview.append(L" ");
    title_preview.append(editable_fields_set_->address.GetFieldText(
        AutoFillType(ADDRESS_HOME_LINE1)));
  } else {
    title = editable_fields_set_->credit_card.Label();
    if (title.empty())
      title = l10n_util::GetString(IDS_AUTOFILL_NEW_CREDITCARD);
  }
  expand_item_button_ = new views::ImageButton(this);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap *image = NULL;
  if (editable_fields_set_->is_opened) {
    // TODO(georgey) fix this when resources available.
    image = rb.GetBitmapNamed(ThemeResourcesUtil::GetId("app_droparrow"));
  } else {
    // TODO(georgey) fix this when resources available.
    image = rb.GetBitmapNamed(ThemeResourcesUtil::GetId("arrow_right"));
    title_label_preview_ = new views::Label(title_preview);
  }
  expand_item_button_->SetImage(views::CustomButton::BS_NORMAL, image);
  title_label_ = new views::Label(title);
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
  if (editable_fields_set_->is_opened) {
    expand_item_button_->set_background(
        views::Background::CreateSolidBackground(bk_color));
    title_label_->set_background(
        views::Background::CreateSolidBackground(bk_color));
  }
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  layout->StartRow(0, three_column_header_);
  layout->AddView(expand_item_button_);
  if (editable_fields_set_->is_opened) {
    layout->AddView(title_label_, 3, 1);
  } else {
    layout->AddView(title_label_);
    layout->AddView(title_label_preview_);
  }
}

void AutoFillProfilesView::EditableSetViewContents::InitAddressFields(
    views::GridLayout* layout) {
  DCHECK(editable_fields_set_->is_address);

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
  DCHECK(billing_model_);
  DCHECK(shipping_model_);

  for (int field = 0; field < arraysize(credit_card_fields_); ++field) {
    DCHECK(!text_fields_[credit_card_fields_[field].text_field]);
    text_fields_[credit_card_fields_[field].text_field] =
        new views::Textfield(views::Textfield::STYLE_DEFAULT);
    text_fields_[credit_card_fields_[field].text_field]->SetController(this);
    if (credit_card_fields_[field].text_field == TEXT_LABEL)
      text_fields_[TEXT_LABEL]->SetText(
          editable_fields_set_->credit_card.Label());
    else
      text_fields_[credit_card_fields_[field].text_field]->SetText(
          editable_fields_set_->credit_card.GetFieldText(
          AutoFillType(credit_card_fields_[field].type)));
  }

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_LABEL));
  layout->StartRow(0, triple_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_LABEL]);
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_NAME_ON_CARD));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(text_fields_[TEXT_CC_NAME]);
  // Address combo boxes.
  combo_box_billing_ = new views::Combobox(billing_model_);
  combo_box_billing_->set_listener(this);
  combo_box_billing_->SetSelectedItem(
      billing_model_->GetIndex(
      editable_fields_set_->credit_card.billing_address()));
  billing_model_->UsedWithComboBox(combo_box_billing_);

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_BILLING_ADDRESS));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(combo_box_billing_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  combo_box_shipping_ = new views::Combobox(shipping_model_);
  combo_box_shipping_->set_listener(this);
  if (editable_fields_set_->credit_card.shipping_address() ==
      editable_fields_set_->credit_card.billing_address()) {
    // The addresses are the same, so use "the same address" label.
    combo_box_shipping_->SetSelectedItem(0);
  } else {
    combo_box_shipping_->SetSelectedItem(
        shipping_model_->GetIndex(
        editable_fields_set_->credit_card.shipping_address()));
  }
  shipping_model_->UsedWithComboBox(combo_box_shipping_);

  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(CreateLeftAlignedLabel(IDS_AUTOFILL_DIALOG_SHIPPING_ADDRESS));
  layout->StartRow(0, double_column_fill_view_set_id_);
  layout->AddView(combo_box_shipping_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Layout credit card info
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

  layout->StartRow(0, triple_column_leading_view_set_id_);
  layout->AddView(delete_button_);
}

void AutoFillProfilesView::EditableSetViewContents::InitLayoutGrid(
    views::GridLayout* layout) {
  views::ColumnSet* column_set =
      layout->AddColumnSet(double_column_fill_view_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  int i;
  for (i = 0; i < 2; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                          views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(double_column_leading_view_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  for (i = 0; i < 2; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1, views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(triple_column_fill_view_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  for (i = 0; i < 3; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                          views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(triple_column_leading_view_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
  for (i = 0; i < 3; ++i) {
    if (i)
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1, views::GridLayout::USE_PREF, 0, 0);
  }
  column_set = layout->AddColumnSet(four_column_city_state_zip_set_id_);
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
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
  column_set->AddPaddingColumn(0, kPanelHorizIndentation);
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

  column_set = layout->AddColumnSet(three_column_header_);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                        0, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
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
    views::Combobox *combo_box) {
  DCHECK(address_labels_);
  combo_boxes_.push_back(combo_box);
}

void AutoFillProfilesView::AddressComboBoxModel::LabelChanged() {
  DCHECK(address_labels_);
  for (std::list<views::Combobox *>::iterator it = combo_boxes_.begin();
       it != combo_boxes_.end();
       ++it)
    (*it)->ModelChanged();
}

int AutoFillProfilesView::AddressComboBoxModel::GetIndex(const string16 &s) {
  int shift = is_billing_ ? 0 : 1;
  DCHECK(address_labels_);
  for (size_t i = 0; i < address_labels_->size(); ++i) {
    DCHECK(address_labels_->at(i).is_address);
    if (address_labels_->at(i).address.Label() == s)
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
  std::wstring label = address_labels_->at(index - shift).address.Label();
  if (label.empty())
    label = l10n_util::GetString(IDS_AUTOFILL_NEW_ADDRESS);
  return label;
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, public:
AutoFillProfilesView::ScrollViewContents::ScrollViewContents(
    AutoFillProfilesView* observer,
    std::vector<EditableSetInfo>* profiles,
    std::vector<EditableSetInfo>* credit_cards)
    : profiles_(profiles),
      credit_cards_(credit_cards),
      add_address_(NULL),
      add_credit_card_(NULL),
      observer_(observer),
      billing_model_(true),
      shipping_model_(false) {
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
    SetBounds(x(), y(), width, height);
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
    Init();
  }
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents,
// views::ButtonListener implementations
void AutoFillProfilesView::ScrollViewContents::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == add_address_)
    observer_->AddClicked(EDITABLE_SET_ADDRESS);
  else if (sender == add_credit_card_)
    observer_->AddClicked(EDITABLE_SET_CREDIT_CARD);
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::ScrollViewContents, private
void AutoFillProfilesView::ScrollViewContents::Init() {
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
        new EditableSetViewContents(observer_, &billing_model_,
                                    &shipping_model_, it);
    layout->StartRow(0, single_column_filled_view_set_id);
    layout->AddView(address_view);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  billing_model_.set_address_labels(profiles_);
  shipping_model_.set_address_labels(profiles_);

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
        new EditableSetViewContents(observer_, &billing_model_,
                                    &shipping_model_, it);
    layout->StartRow(0, single_column_filled_view_set_id);
    layout->AddView(address_view);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  add_credit_card_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_AUTOFILL_ADD_CREDITCARD_BUTTON));

  layout->StartRow(0, single_column_left_view_set_id);
  layout->AddView(add_credit_card_);
}

/////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView::AutoFillScrollView, public:
AutoFillProfilesView::AutoFillScrollView::AutoFillScrollView(
    AutoFillProfilesView* observer,
    std::vector<EditableSetInfo>* profiles,
    std::vector<EditableSetInfo>* credit_cards)
    : scroll_view_(new views::ScrollView),
      scroll_contents_view_(
          new ScrollViewContents(observer, profiles, credit_cards)),
      profiles_(profiles),
      credit_cards_(credit_cards),
      observer_(observer) {
  AddChildView(scroll_view_);
  // After the following call, |scroll_view_| owns |scroll_contents_view_|
  // and deletes it when it gets deleted or reset.
  scroll_view_->SetContents(scroll_contents_view_);
  set_background(new ListBackground());
}

void AutoFillProfilesView::AutoFillScrollView::RebuildView() {
  gfx::Rect visible_rectangle = scroll_view_->GetVisibleRect();
  scroll_contents_view_ = new ScrollViewContents(observer_,
                                                 profiles_,
                                                 credit_cards_);
  // Deletes the old contents view and takes ownership of
  // |scroll_contents_view_|.
  scroll_view_->SetContents(scroll_contents_view_);
  scroll_contents_view_->ScrollRectToVisible(visible_rectangle.x(),
                                             visible_rectangle.y(),
                                             visible_rectangle.width(),
                                             visible_rectangle.height());
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

