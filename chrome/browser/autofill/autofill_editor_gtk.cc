// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Creates a label whose text is set from the resource id |label_id|.
GtkWidget* CreateLabel(int label_id) {
  GtkWidget* label = gtk_label_new(l10n_util::GetStringUTF8(label_id).c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  return label;
}

// Sets the text of |entry| to the value of the field |type| from |profile|.
void SetEntryText(GtkWidget* entry, FormGroup* profile, _FieldType type) {
  gtk_entry_set_text(
      GTK_ENTRY(entry),
      UTF16ToUTF8(profile->GetFieldText(AutoFillType(type))).c_str());
}

// Returns the current value of |entry|.
string16 GetEntryText(GtkWidget* entry) {
  return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(entry)));
}

// Sets |form|'s field of type |type| to the text in |entry|.
void SetFormValue(GtkWidget* entry, FormGroup* form, _FieldType type) {
  form->SetInfo(AutoFillType(type), GetEntryText(entry));
}

// Sets the number of characters to display in |combobox| to |width|.
void SetComboBoxCellRendererCharWidth(GtkWidget* combobox, int width) {
  GList* cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(combobox));
  DCHECK(g_list_length(cells) > 0);
  GtkCellRendererText* renderer =
      GTK_CELL_RENDERER_TEXT(g_list_first(cells)->data);
  g_object_set(G_OBJECT(renderer), "width-chars", width, NULL);
  g_list_free(cells);
}

// TableBuilder ----------------------------------------------------------------

// A convenience used in populating a GtkTable. To use it create a TableBuilder
// and repeatedly invoke AddWidget. TableBuilder keeps track of the current
// row/col. You can increment the row explicitly by invoking |increment_row|.
class TableBuilder {
 public:
  TableBuilder(int row_count, int col_count);
  ~TableBuilder();

  GtkWidget* table() { return table_; }

  void increment_row() {
    row_++;
    col_ = 0;
  }

  GtkWidget* AddWidget(GtkWidget* widget, int col_span);

  void set_x_padding(int x) { x_padding_ = x; }
  void set_y_padding(int y) { y_padding_ = y; }

  void reset_padding() {
    x_padding_ = y_padding_ = gtk_util::kControlSpacing / 2;
  }

 private:
  GtkWidget* table_;

  // Number of rows/columns.
  const int row_count_;
  const int col_count_;

  // Current row/column.
  int row_;
  int col_;

  // Padding.
  int x_padding_;
  int y_padding_;

  DISALLOW_COPY_AND_ASSIGN(TableBuilder);
};

TableBuilder::TableBuilder(int row_count, int col_count)
    : table_(gtk_table_new(row_count, col_count, FALSE)),
      row_count_(row_count),
      col_count_(col_count),
      row_(0),
      col_(0),
      x_padding_(gtk_util::kControlSpacing / 2),
      y_padding_(gtk_util::kControlSpacing / 2) {
}

TableBuilder::~TableBuilder() {
  DCHECK_EQ(row_count_, row_);
}

GtkWidget* TableBuilder::AddWidget(GtkWidget* widget, int col_span) {
  gtk_table_attach(
      GTK_TABLE(table_),
      widget,
      col_, col_ + col_span,
      row_, row_ + 1,
      // The next line makes the widget expand to take up any extra horizontal
      // space.
      static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND),
      GTK_FILL,
      x_padding_, y_padding_);
  col_ += col_span;
  if (col_ == col_count_) {
    row_++;
    col_ = 0;
  }
  return widget;
}

// Returns true if the text contained in the entry |widget| is non-empty and
// parses as a valid phone number.
bool IsValidPhoneNumber(GtkWidget* widget) {
  string16 text(GetEntryText(widget));
  if (text.empty())
    return true;

  string16 number, city_code, country_code;
  return PhoneNumber::ParsePhoneNumber(text, &number, &city_code,&country_code);
}

// AutoFillProfileEditor -------------------------------------------------------

// Class responsible for editing or creating an AutoFillProfile.
class AutoFillProfileEditor {
 public:
  AutoFillProfileEditor(AutoFillDialogObserver* observer,
                        Profile* profile,
                        AutoFillProfile* auto_fill_profile);

 private:
  friend class DeleteTask<AutoFillProfileEditor>;

  virtual ~AutoFillProfileEditor() {}

  void Init();

  // Registers for the text changed on all our text fields.
  void RegisterForTextChanged();

  // Sets the values of the various widgets to |profile|.
  void SetWidgetValues(AutoFillProfile* profile);

  // Notifies the observer of the new changes. This either updates the current
  // AutoFillProfile or creates a new one.
  void ApplyEdits();

  // Sets the various form fields in |profile| to match the values in the
  // widgets.
  void SetProfileValuesFromWidgets(AutoFillProfile* profile);

  // Updates the image displayed by |image| depending upon whether the text in
  // |entry| is a valid phone number.
  void UpdatePhoneImage(GtkWidget* entry, GtkWidget* image);

  // Sets the size request for the widget to match the size of the good/bad
  // images. We must do this as the image of the phone widgets is set to null
  // when not empty.
  void SetPhoneSizeRequest(GtkWidget* widget);

  // Updates the enabled state of the ok button.
  void UpdateOkButton();

  CHROMEGTK_CALLBACK_0(AutoFillProfileEditor, void, OnDestroy);
  CHROMEG_CALLBACK_1(AutoFillProfileEditor, void, OnResponse, GtkDialog*, gint);
  CHROMEG_CALLBACK_0(AutoFillProfileEditor, void, OnPhoneChanged, GtkEditable*);
  CHROMEG_CALLBACK_0(AutoFillProfileEditor, void, OnFaxChanged, GtkEditable*);
  CHROMEG_CALLBACK_0(AutoFillProfileEditor, void, OnFieldChanged, GtkEditable*);

  // Are we creating a new profile?
  const bool is_new_;

  // If is_new_ is false this is the unique id of the profile the user is
  // editing.
  const std::string profile_guid_;

  AutoFillDialogObserver* observer_;

  Profile* profile_;

  GtkWidget* dialog_;
  GtkWidget* full_name_;
  GtkWidget* company_;
  GtkWidget* address_1_;
  GtkWidget* address_2_;
  GtkWidget* city_;
  GtkWidget* state_;
  GtkWidget* zip_;
  GtkWidget* country_;
  GtkWidget* phone_;
  GtkWidget* phone_image_;
  GtkWidget* fax_;
  GtkWidget* fax_image_;
  GtkWidget* email_;
  GtkWidget* ok_button_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillProfileEditor);
};

AutoFillProfileEditor::AutoFillProfileEditor(
    AutoFillDialogObserver* observer,
    Profile* profile,
    AutoFillProfile* auto_fill_profile)
    : is_new_(!auto_fill_profile ? true : false),
      profile_guid_(auto_fill_profile ? auto_fill_profile->guid()
          : std::string()),
      observer_(observer),
      profile_(profile) {
  Init();

  if (auto_fill_profile)
    SetWidgetValues(auto_fill_profile);

  RegisterForTextChanged();

  UpdateOkButton();

  gtk_util::ShowDialogWithLocalizedSize(
      dialog_,
      IDS_AUTOFILL_DIALOG_EDIT_ADDRESS_WIDTH_CHARS,
      IDS_AUTOFILL_DIALOG_EDIT_ADDRESS_HEIGHT_LINES,
      true);
  gtk_util::PresentWindow(dialog_, gtk_get_current_event_time());
}

void AutoFillProfileEditor::Init() {
  TableBuilder main_table_builder(15, 2);

  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_FULL_NAME), 2);
  full_name_ = main_table_builder.AddWidget(gtk_entry_new(), 1);
  main_table_builder.increment_row();

  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_COMPANY_NAME),
                               2);
  company_ = main_table_builder.AddWidget(gtk_entry_new(), 1);
  main_table_builder.increment_row();

  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1),
                               2);
  address_1_ = main_table_builder.AddWidget(gtk_entry_new(), 1);
  main_table_builder.increment_row();

  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2),
                               2);
  address_2_ = main_table_builder.AddWidget(gtk_entry_new(), 1);
  main_table_builder.increment_row();

  TableBuilder city_state_builder(2, 3);
  city_state_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_CITY), 1);
  city_state_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_STATE), 1);
  city_state_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_ZIP_CODE), 1);

  city_ = city_state_builder.AddWidget(gtk_entry_new(), 1);
  state_ = city_state_builder.AddWidget(gtk_entry_new(), 1);
  zip_ = city_state_builder.AddWidget(gtk_entry_new(), 1);

  main_table_builder.set_x_padding(0);
  main_table_builder.set_y_padding(0);
  main_table_builder.AddWidget(city_state_builder.table(), 2);
  main_table_builder.reset_padding();

  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_COUNTRY), 2);
  country_ = main_table_builder.AddWidget(gtk_entry_new(), 1);
  main_table_builder.increment_row();

  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_PHONE), 1);
  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_FAX), 1);

  GtkWidget* phone_box =
      main_table_builder.AddWidget(gtk_hbox_new(FALSE, 0), 1);
  gtk_box_set_spacing(GTK_BOX(phone_box), gtk_util::kControlSpacing);
  phone_ = gtk_entry_new();
  g_signal_connect(phone_, "changed", G_CALLBACK(OnPhoneChangedThunk), this);
  gtk_box_pack_start(GTK_BOX(phone_box), phone_, TRUE, TRUE, 0);
  phone_image_ = gtk_image_new_from_pixbuf(NULL);
  SetPhoneSizeRequest(phone_image_);
  gtk_box_pack_start(GTK_BOX(phone_box), phone_image_, FALSE, FALSE, 0);

  GtkWidget* fax_box =
      main_table_builder.AddWidget(gtk_hbox_new(FALSE, 0), 1);
  gtk_box_set_spacing(GTK_BOX(fax_box), gtk_util::kControlSpacing);
  fax_ = gtk_entry_new();
  g_signal_connect(fax_, "changed", G_CALLBACK(OnFaxChangedThunk), this);
  gtk_box_pack_start(GTK_BOX(fax_box), fax_, TRUE, TRUE, 0);
  fax_image_ = gtk_image_new_from_pixbuf(NULL);
  SetPhoneSizeRequest(fax_image_);
  gtk_box_pack_start(GTK_BOX(fax_box), fax_image_, FALSE, FALSE, 0);

  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_EMAIL), 2);
  email_ = main_table_builder.AddWidget(gtk_entry_new(), 1);
  main_table_builder.increment_row();

  int caption_id = is_new_ ? IDS_AUTOFILL_ADD_ADDRESS_CAPTION :
                             IDS_AUTOFILL_EDIT_ADDRESS_CAPTION;
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(caption_id).c_str(),
      NULL,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      NULL);

  ok_button_ = gtk_dialog_add_button(GTK_DIALOG(dialog_),
                                     GTK_STOCK_APPLY,
                                     GTK_RESPONSE_APPLY);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_APPLY);

  gtk_dialog_add_button(GTK_DIALOG(dialog_), GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroyThunk), this);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                     main_table_builder.table(), TRUE, TRUE, 0);
}

void AutoFillProfileEditor::RegisterForTextChanged() {
  g_signal_connect(full_name_, "changed", G_CALLBACK(OnFieldChangedThunk),
                   this);
  g_signal_connect(company_, "changed", G_CALLBACK(OnFieldChangedThunk),
                   this);
  g_signal_connect(address_1_, "changed", G_CALLBACK(OnFieldChangedThunk),
                   this);
  g_signal_connect(address_2_, "changed", G_CALLBACK(OnFieldChangedThunk),
                   this);
  g_signal_connect(city_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
  g_signal_connect(state_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
  g_signal_connect(zip_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
  g_signal_connect(country_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
  g_signal_connect(email_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
  g_signal_connect(phone_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
  g_signal_connect(fax_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
}

void AutoFillProfileEditor::SetWidgetValues(AutoFillProfile* profile) {
  SetEntryText(full_name_, profile, NAME_FULL);
  SetEntryText(company_, profile, COMPANY_NAME);
  SetEntryText(address_1_, profile, ADDRESS_HOME_LINE1);
  SetEntryText(address_2_, profile, ADDRESS_HOME_LINE2);
  SetEntryText(city_, profile, ADDRESS_HOME_CITY);
  SetEntryText(state_, profile, ADDRESS_HOME_STATE);
  SetEntryText(zip_, profile, ADDRESS_HOME_ZIP);
  SetEntryText(country_, profile, ADDRESS_HOME_COUNTRY);
  SetEntryText(phone_, profile, PHONE_HOME_WHOLE_NUMBER);
  SetEntryText(fax_, profile, PHONE_FAX_WHOLE_NUMBER);
  SetEntryText(email_, profile, EMAIL_ADDRESS);

  UpdatePhoneImage(phone_, phone_image_);
  UpdatePhoneImage(fax_, fax_image_);
}

void AutoFillProfileEditor::ApplyEdits() {
  // Build the current set of profiles.
  std::vector<AutoFillProfile> profiles;
  PersonalDataManager* data_manager = profile_->GetPersonalDataManager();
  for (std::vector<AutoFillProfile*>::const_iterator i =
           data_manager->profiles().begin();
       i != data_manager->profiles().end(); ++i) {
    profiles.push_back(**i);
  }
  AutoFillProfile* profile = NULL;

  if (!is_new_) {
    // The user is editing an existing profile, find it.
    for (std::vector<AutoFillProfile>::iterator i = profiles.begin();
         i != profiles.end(); ++i) {
      if (i->guid() == profile_guid_) {
        profile = &(*i);
        break;
      }
    }
    DCHECK(profile);  // We should have found a profile, if not we'll end up
                      // creating one below.
  }

  if (!profile) {
    profiles.push_back(AutoFillProfile());
    profile = &profiles.back();
  }

  // Update the values in the profile.
  SetProfileValuesFromWidgets(profile);

  // And apply the edits.
  observer_->OnAutoFillDialogApply(&profiles, NULL);
}

void AutoFillProfileEditor::SetProfileValuesFromWidgets(
    AutoFillProfile* profile) {
  SetFormValue(full_name_, profile, NAME_FULL);
  SetFormValue(company_, profile, COMPANY_NAME);
  SetFormValue(address_1_, profile, ADDRESS_HOME_LINE1);
  SetFormValue(address_2_, profile, ADDRESS_HOME_LINE2);
  SetFormValue(city_, profile, ADDRESS_HOME_CITY);
  SetFormValue(state_, profile, ADDRESS_HOME_STATE);
  SetFormValue(zip_, profile, ADDRESS_HOME_ZIP);
  SetFormValue(country_, profile, ADDRESS_HOME_COUNTRY);
  SetFormValue(email_, profile, EMAIL_ADDRESS);

  string16 number, city_code, country_code;
  PhoneNumber::ParsePhoneNumber(
      GetEntryText(phone_), &number, &city_code, &country_code);
  profile->SetInfo(AutoFillType(PHONE_HOME_COUNTRY_CODE), country_code);
  profile->SetInfo(AutoFillType(PHONE_HOME_CITY_CODE), city_code);
  profile->SetInfo(AutoFillType(PHONE_HOME_NUMBER), number);

  PhoneNumber::ParsePhoneNumber(
      GetEntryText(fax_), &number, &city_code, &country_code);
  profile->SetInfo(AutoFillType(PHONE_FAX_COUNTRY_CODE), country_code);
  profile->SetInfo(AutoFillType(PHONE_FAX_CITY_CODE), city_code);
  profile->SetInfo(AutoFillType(PHONE_FAX_NUMBER), number);
}

void AutoFillProfileEditor::UpdatePhoneImage(GtkWidget* entry,
                                             GtkWidget* image) {
  string16 number, city_code, country_code;
  string16 text(GetEntryText(entry));
  if (text.empty()) {
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), NULL);
  } else if (PhoneNumber::ParsePhoneNumber(text, &number, &city_code,
                                           &country_code)) {
    gtk_image_set_from_pixbuf(GTK_IMAGE(image),
        ResourceBundle::GetSharedInstance().GetPixbufNamed(
            IDR_INPUT_GOOD));
  } else {
    gtk_image_set_from_pixbuf(GTK_IMAGE(image),
        ResourceBundle::GetSharedInstance().GetPixbufNamed(
            IDR_INPUT_ALERT));
  }
}

void AutoFillProfileEditor::SetPhoneSizeRequest(GtkWidget* widget) {
  GdkPixbuf* buf =
      ResourceBundle::GetSharedInstance().GetPixbufNamed(IDR_INPUT_ALERT);
  gtk_widget_set_size_request(widget,
                              gdk_pixbuf_get_width(buf),
                              gdk_pixbuf_get_height(buf));
}

void AutoFillProfileEditor::UpdateOkButton() {
  // Enable the ok button if at least one field is non-empty and the phone
  // numbers are valid.
  bool valid =
      !GetEntryText(full_name_).empty() ||
      !GetEntryText(company_).empty() ||
      !GetEntryText(address_1_).empty() ||
      !GetEntryText(address_2_).empty() ||
      !GetEntryText(city_).empty() ||
      !GetEntryText(state_).empty() ||
      !GetEntryText(zip_).empty() ||
      !GetEntryText(country_).empty() ||
      !GetEntryText(email_).empty();
  if (valid) {
    valid = IsValidPhoneNumber(phone_) && IsValidPhoneNumber(fax_);
  } else {
    valid = IsValidPhoneNumber(phone_) && IsValidPhoneNumber(fax_) &&
        (!GetEntryText(full_name_).empty() ||
         !GetEntryText(company_).empty());
  }
  gtk_widget_set_sensitive(ok_button_, valid);
}

void AutoFillProfileEditor::OnDestroy(GtkWidget* widget) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoFillProfileEditor::OnResponse(GtkDialog* dialog, gint response_id) {
  if (response_id == GTK_RESPONSE_APPLY || response_id == GTK_RESPONSE_OK)
    ApplyEdits();

  if (response_id == GTK_RESPONSE_OK ||
      response_id == GTK_RESPONSE_APPLY ||
      response_id == GTK_RESPONSE_CANCEL ||
      response_id == GTK_RESPONSE_DELETE_EVENT) {
    gtk_widget_destroy(GTK_WIDGET(dialog));
  }
}

void AutoFillProfileEditor::OnPhoneChanged(GtkEditable* editable) {
  UpdatePhoneImage(phone_, phone_image_);
}

void AutoFillProfileEditor::OnFaxChanged(GtkEditable* editable) {
  UpdatePhoneImage(fax_, fax_image_);
}

void AutoFillProfileEditor::OnFieldChanged(GtkEditable* editable) {
  UpdateOkButton();
}

// AutoFillCreditCardEditor ----------------------------------------------------

// Number of years shown in the expiration year combobox.
const int kNumYears = 10;

// Class responsible for editing/creating a CreditCard.
class AutoFillCreditCardEditor {
 public:
  AutoFillCreditCardEditor(AutoFillDialogObserver* observer,
                           Profile* profile,
                           CreditCard* credit_card);

 private:
  friend class DeleteTask<AutoFillCreditCardEditor>;

  virtual ~AutoFillCreditCardEditor() {}

  // Creates the combobox for choosing the month.
  GtkWidget* CreateMonthWidget();

  // Creates the combobox for choosing the year.
  GtkWidget* CreateYearWidget();

  void Init();

  // Registers for the text changed on all our text fields.
  void RegisterForTextChanged();

  // Sets the values displayed in the various widgets from |profile|.
  void SetWidgetValues(CreditCard* profile);

  // Updates the observer with the CreditCard being edited.
  void ApplyEdits();

  // Updates |card| with the values from the widgets.
  void SetCreditCardValuesFromWidgets(CreditCard* card);

  // Updates the enabled state of the ok button.
  void UpdateOkButton();

  CHROMEGTK_CALLBACK_0(AutoFillCreditCardEditor, void, OnDestroy);
  CHROMEG_CALLBACK_1(AutoFillCreditCardEditor, void, OnResponse, GtkDialog*,
                     gint);
  CHROMEG_CALLBACK_0(AutoFillCreditCardEditor, void, OnFieldChanged,
                     GtkEditable*);
  CHROMEG_CALLBACK_2(AutoFillCreditCardEditor, void, OnDeleteTextFromNumber,
                     GtkEditable*, gint, gint);
  CHROMEG_CALLBACK_3(AutoFillCreditCardEditor, void, OnInsertTextIntoNumber,
                     GtkEditable*, gchar*, gint, gint*);

  // Are we creating a new credit card?
  const bool is_new_;

  // If is_new_ is false this is the unique id of the credit card the user is
  // editing.
  const std::string credit_card_guid_;

  AutoFillDialogObserver* observer_;

  Profile* profile_;

  // Initial year shown in expiration date year combobox.
  int base_year_;

  // Has the number_ field been edited by the user yet?
  bool edited_number_;

  GtkWidget* dialog_;
  GtkWidget* name_;
  GtkWidget* number_;
  GtkWidget* month_;
  GtkWidget* year_;
  GtkWidget* ok_button_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillCreditCardEditor);
};

AutoFillCreditCardEditor::AutoFillCreditCardEditor(
    AutoFillDialogObserver* observer,
    Profile* profile,
    CreditCard* credit_card)
    : is_new_(!credit_card ? true : false),
      credit_card_guid_(credit_card ? credit_card->guid() : std::string()),
      observer_(observer),
      profile_(profile),
      base_year_(0),
      edited_number_(false) {
  base::Time::Exploded exploded_time;
  base::Time::Now().LocalExplode(&exploded_time);
  base_year_ = exploded_time.year;

  Init();

  if (credit_card) {
    SetWidgetValues(credit_card);
  } else {
    // We're creating a new credit card. Select January of next year by default.
    gtk_combo_box_set_active(GTK_COMBO_BOX(month_), 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(year_), 1);
  }

  RegisterForTextChanged();

  UpdateOkButton();

  gtk_util::ShowDialogWithLocalizedSize(
      dialog_,
      IDS_AUTOFILL_DIALOG_EDIT_CCARD_WIDTH_CHARS,
      IDS_AUTOFILL_DIALOG_EDIT_CCARD_HEIGHT_LINES,
      true);
  gtk_util::PresentWindow(dialog_, gtk_get_current_event_time());
}

GtkWidget* AutoFillCreditCardEditor::CreateMonthWidget() {
  GtkWidget* combobox = gtk_combo_box_new_text();
  for (int i = 1; i <= 12; ++i) {
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox),
                              StringPrintf("%02i", i).c_str());
  }

  SetComboBoxCellRendererCharWidth(combobox, 2);
  return combobox;
}

GtkWidget* AutoFillCreditCardEditor::CreateYearWidget() {
  GtkWidget* combobox = gtk_combo_box_new_text();
  for (int i = 0; i < kNumYears; ++i) {
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox),
                              StringPrintf("%04i", i + base_year_).c_str());
  }

  SetComboBoxCellRendererCharWidth(combobox, 4);
  return combobox;
}

void AutoFillCreditCardEditor::Init() {
  TableBuilder main_table_builder(6, 2);

  main_table_builder.AddWidget(
      CreateLabel(IDS_AUTOFILL_DIALOG_NAME_ON_CARD), 2);
  name_ = main_table_builder.AddWidget(gtk_entry_new(), 2);

  main_table_builder.AddWidget(
      CreateLabel(IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER), 2);
  number_ = main_table_builder.AddWidget(gtk_entry_new(), 1);
  gtk_entry_set_width_chars(GTK_ENTRY(number_), 20);
  // Add an empty widget purely for spacing to match the mocks.
  main_table_builder.AddWidget(gtk_hbox_new(FALSE, 0), 1);

  main_table_builder.AddWidget(CreateLabel(IDS_AUTOFILL_DIALOG_EXPIRATION_DATE),
                               2);
  GtkWidget* box = main_table_builder.AddWidget(
      gtk_hbox_new(FALSE, gtk_util::kControlSpacing), 1);
  month_ = CreateMonthWidget();
  gtk_box_pack_start(GTK_BOX(box), month_, FALSE, FALSE, 0);
  year_ = CreateYearWidget();
  gtk_box_pack_start(GTK_BOX(box), year_, FALSE, FALSE, 0);
  main_table_builder.increment_row();

  int caption_id = is_new_ ? IDS_AUTOFILL_ADD_CREDITCARD_CAPTION :
                             IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION;
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(caption_id).c_str(),
      NULL,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      NULL);

  ok_button_ = gtk_dialog_add_button(GTK_DIALOG(dialog_),
                                     GTK_STOCK_APPLY,
                                     GTK_RESPONSE_APPLY);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_APPLY);

  gtk_dialog_add_button(GTK_DIALOG(dialog_), GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroyThunk), this);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                     main_table_builder.table(), TRUE, TRUE, 0);
}

void AutoFillCreditCardEditor::RegisterForTextChanged() {
  g_signal_connect(name_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
  g_signal_connect(number_, "changed", G_CALLBACK(OnFieldChangedThunk), this);
  g_signal_connect(number_, "delete-text",
                   G_CALLBACK(OnDeleteTextFromNumberThunk), this);
  g_signal_connect(number_, "insert-text",
                   G_CALLBACK(OnInsertTextIntoNumberThunk), this);
}

void AutoFillCreditCardEditor::SetWidgetValues(CreditCard* card) {
  SetEntryText(name_, card, CREDIT_CARD_NAME);

  gtk_entry_set_text(GTK_ENTRY(number_),
                     UTF16ToUTF8(card->ObfuscatedNumber()).c_str());

  int month;
  base::StringToInt(card->GetFieldText(AutoFillType(CREDIT_CARD_EXP_MONTH)),
                    &month);
  if (month >= 1 && month <= 12) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(month_), month - 1);
  } else {
    gtk_combo_box_set_active(GTK_COMBO_BOX(month_), 0);
  }

  int year;
  if (!base::StringToInt(
          card->GetFieldText(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)),
          &year)) {
    NOTREACHED();
  }
  if (year >= base_year_ && year < base_year_ + kNumYears)
    gtk_combo_box_set_active(GTK_COMBO_BOX(year_), year - base_year_);
  else
    gtk_combo_box_set_active(GTK_COMBO_BOX(year_), 0);
}

void AutoFillCreditCardEditor::ApplyEdits() {
  // Build a vector of the current set of credit cards.
  PersonalDataManager* data_manager = profile_->GetPersonalDataManager();
  std::vector<CreditCard> cards;
  for (std::vector<CreditCard*>::const_iterator i =
           data_manager->credit_cards().begin();
       i != data_manager->credit_cards().end(); ++i) {
    cards.push_back(**i);
  }

  CreditCard* card = NULL;

  if (!is_new_) {
    // The user is editing an existing credit card, find it.
    for (std::vector<CreditCard>::iterator i = cards.begin();
         i != cards.end(); ++i) {
      if (i->guid() == credit_card_guid_) {
        card = &(*i);
        break;
      }
    }
    DCHECK(card);  // We should have found the credit card we were created with,
                   // if not we'll end up creating one below.
  }

  if (!card) {
    cards.push_back(CreditCard());
    card = &cards.back();
  }

  // Update the credit card from what the user typed in.
  SetCreditCardValuesFromWidgets(card);

  // And update the model.
  observer_->OnAutoFillDialogApply(NULL, &cards);
}

void AutoFillCreditCardEditor::SetCreditCardValuesFromWidgets(
    CreditCard* card) {
  SetFormValue(name_, card, CREDIT_CARD_NAME);

  if (edited_number_)
    SetFormValue(number_, card, CREDIT_CARD_NUMBER);

  int selected_month_index =
      gtk_combo_box_get_active(GTK_COMBO_BOX(month_));
  if (selected_month_index == -1)
    selected_month_index = 0;
  card->SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH),
                base::IntToString16(selected_month_index + 1));

  int selected_year_index =
      gtk_combo_box_get_active(GTK_COMBO_BOX(year_));
  if (selected_year_index == -1)
    selected_year_index = 0;
  card->SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                base::IntToString16(selected_year_index + base_year_));
}

void AutoFillCreditCardEditor::UpdateOkButton() {
  // Enable the ok button if at least one field is non-empty and the phone
  // numbers are valid.
  bool valid = !GetEntryText(name_).empty() || !GetEntryText(number_).empty();
  gtk_widget_set_sensitive(ok_button_, valid);
}

void AutoFillCreditCardEditor::OnDestroy(GtkWidget* widget) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoFillCreditCardEditor::OnResponse(GtkDialog* dialog, gint response_id) {
  if (response_id == GTK_RESPONSE_APPLY || response_id == GTK_RESPONSE_OK)
    ApplyEdits();

  if (response_id == GTK_RESPONSE_OK ||
      response_id == GTK_RESPONSE_APPLY ||
      response_id == GTK_RESPONSE_CANCEL ||
      response_id == GTK_RESPONSE_DELETE_EVENT) {
    gtk_widget_destroy(GTK_WIDGET(dialog));
  }
}

void AutoFillCreditCardEditor::OnFieldChanged(GtkEditable* editable) {
  if (editable == GTK_EDITABLE(number_))
    edited_number_ = true;
  UpdateOkButton();
}

void AutoFillCreditCardEditor::OnDeleteTextFromNumber(GtkEditable* editable,
                                                      gint start,
                                                      gint end) {
  if (!edited_number_) {
    // The user hasn't deleted any text yet, so delete it all.
    edited_number_ = true;
    g_signal_stop_emission_by_name(editable, "delete-text");
    gtk_entry_set_text(GTK_ENTRY(number_), "");
  }
}

void AutoFillCreditCardEditor::OnInsertTextIntoNumber(GtkEditable* editable,
                                                      gchar* new_text,
                                                      gint new_text_length,
                                                      gint* position) {
  if (!edited_number_) {
    // This is the first edit to the number. If |editable| is not empty, reset
    // the text so that any obfuscated text is removed.
    edited_number_ = true;

    if (GetEntryText(GTK_WIDGET(editable)).empty())
      return;

    g_signal_stop_emission_by_name(editable, "insert-text");

    if (new_text_length < 0)
      new_text_length = strlen(new_text);
    gtk_entry_set_text(GTK_ENTRY(number_),
                       std::string(new_text, new_text_length).c_str());

    // Sets the cursor after the last character in |editable|.
    gtk_editable_set_position(editable, -1);
  }
}

}  // namespace

void ShowAutoFillProfileEditor(gfx::NativeView parent,
                               AutoFillDialogObserver* observer,
                               Profile* profile,
                               AutoFillProfile* auto_fill_profile) {
  // AutoFillProfileEditor takes care of deleting itself.
  new AutoFillProfileEditor(observer, profile, auto_fill_profile);
}

void ShowAutoFillCreditCardEditor(gfx::NativeView parent,
                                  AutoFillDialogObserver* observer,
                                  Profile* profile,
                                  CreditCard* credit_card) {
  // AutoFillCreditCardEditor takes care of deleting itself.
  new AutoFillCreditCardEditor(observer, profile, credit_card);
}
