// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "gfx/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// The name of the object property used to store an entry widget pointer on
// another widget.
const char kButtonDataKey[] = "label-entry";

// How far we indent dialog widgets, in pixels.
const int kAutoFillDialogIndent = 5;

// The resource id for the 'Learn more' link button.
const gint kAutoFillDialogLearnMoreLink = 1;

// All of these widgets are GtkEntrys.
typedef struct _AddressWidgets {
  GtkWidget* label;
  GtkWidget* full_name;
  GtkWidget* email;
  GtkWidget* company_name;
  GtkWidget* address_line1;
  GtkWidget* address_line2;
  GtkWidget* city;
  GtkWidget* state;
  GtkWidget* zipcode;
  GtkWidget* country;
  GtkWidget* phone;
  GtkWidget* fax;
} AddressWidgets;

// All of these widgets are GtkEntrys except for |billing_address|, which is
// a GtkComboBox.
typedef struct _CreditCardWidgets {
  GtkWidget* label;
  GtkWidget* name_on_card;
  GtkWidget* card_number;
  GtkWidget* expiration_month;
  GtkWidget* expiration_year;
  GtkWidget* billing_address;
  GtkWidget* phone;
  string16 original_card_number;
} CreditCardWidgets;

// Adds an alignment around |widget| which indents the widget by |offset|.
GtkWidget* IndentWidget(GtkWidget* widget, int offset) {
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0,
                            offset, 0);
  gtk_container_add(GTK_CONTAINER(alignment), widget);
  return alignment;
}

// Makes sure we use the gtk theme colors by loading the base color of an entry
// widget.
void SetWhiteBackground(GtkWidget* widget) {
  GtkWidget* entry = gtk_entry_new();
  gtk_widget_ensure_style(entry);
  GtkStyle* style = gtk_widget_get_style(entry);
  gtk_widget_modify_bg(widget, GTK_STATE_NORMAL,
                       &style->base[GTK_STATE_NORMAL]);
  gtk_widget_destroy(entry);
}

string16 GetEntryText(GtkWidget* entry) {
  return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(entry)));
}

void SetEntryText(GtkWidget* entry, const string16& text) {
  gtk_entry_set_text(GTK_ENTRY(entry), UTF16ToUTF8(text).c_str());
}

void SetButtonData(GtkWidget* widget, GtkWidget* entry) {
  g_object_set_data(G_OBJECT(widget), kButtonDataKey, entry);
}

GtkWidget* GetButtonData(GtkWidget* widget) {
  return static_cast<GtkWidget*>(
      g_object_get_data(G_OBJECT(widget), kButtonDataKey));
}

////////////////////////////////////////////////////////////////////////////////
// Form Table helpers.
//
// The following functions can be used to create a form with labeled widgets.
//

// Creates a form table with dimensions |rows| x |cols|.
GtkWidget* InitFormTable(int rows, int cols) {
  // We have two table rows per form table row.
  GtkWidget* table = gtk_table_new(rows * 2, cols, false);
  gtk_table_set_row_spacings(GTK_TABLE(table), gtk_util::kControlSpacing);
  gtk_table_set_col_spacings(GTK_TABLE(table), gtk_util::kFormControlSpacing);

  // Leave no space between the label and the widget.
  for (int i = 0; i < rows; i++)
    gtk_table_set_row_spacing(GTK_TABLE(table), i * 2, 0);

  return table;
}

// Sets the label of the form widget at |row|,|col|.  The label is |len| columns
// long.
void FormTableSetLabel(
    GtkWidget* table, int row, int col, int len, int label_id) {
  // We have two table rows per form table row.
  row *= 2;

  std::string text;
  if (label_id)
    text = l10n_util::GetStringUTF8(label_id);
  GtkWidget* label = gtk_label_new(text.c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_table_attach(GTK_TABLE(table), label,
                   col, col + len,  // Left col, right col.
                   row, row + 1,  // Top row, bottom row.
                   GTK_FILL, GTK_FILL,  // Options.
                   0, 0);  // Padding.
}

// Sets the form widget at |row|,|col|.  The widget fills up |len| columns.  If
// |expand| is true, the widget will expand to fill all of the extra space in
// the table row.
void FormTableSetWidget(GtkWidget* table,
                        GtkWidget* widget,
                        int row, int col,
                        int len, bool expand) {
  const GtkAttachOptions expand_option =
      static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND);
  GtkAttachOptions xoption = (expand) ?  expand_option : GTK_FILL;

  // We have two table rows per form table row.
  row *= 2;
  gtk_table_attach(GTK_TABLE(table), widget,
                   col, col + len,  // Left col, right col.
                   row + 1, row + 2,  // Top row, bottom row.
                   xoption, GTK_FILL,  // Options.
                   0, 0);  // Padding.
}

// Adds a labeled entry box to the form table at |row|,|col|.  The entry widget
// fills up |len| columns.  The returned widget is owned by |table| and should
// not be destroyed.
GtkWidget* FormTableAddEntry(
    GtkWidget* table, int row, int col, int len, int label_id) {
  FormTableSetLabel(table, row, col, len, label_id);

  GtkWidget* entry = gtk_entry_new();
  FormTableSetWidget(table, entry, row, col, len, false);

  return entry;
}

// Adds a labeled entry box to the form table that will expand to fill extra
// space in the table row.
GtkWidget* FormTableAddExpandedEntry(
    GtkWidget* table, int row, int col, int len, int label_id) {
  FormTableSetLabel(table, row, col, len, label_id);

  GtkWidget* entry = gtk_entry_new();
  FormTableSetWidget(table, entry, row, col, len, true);

  return entry;
}

// Adds a sized entry box to the form table.  The entry widget width is set to
// |char_len|.
GtkWidget* FormTableAddSizedEntry(
    GtkWidget* table, int row, int col, int char_len, int label_id) {
  GtkWidget* entry = FormTableAddEntry(table, row, col, 1, label_id);
  gtk_entry_set_width_chars(GTK_ENTRY(entry), char_len);
  return entry;
}

// Like FormTableAddEntry, but connects to the 'changed' signal.  |changed| is a
// callback to handle the 'changed' signal that is emitted when the user edits
// the entry.  |expander| is the expander widget that will be sent to the
// callback as the user data.
GtkWidget* FormTableAddLabelEntry(
    GtkWidget* table, int row, int col, int len, int label_id,
    GtkWidget* expander, GCallback changed) {
  FormTableSetLabel(table, row, col, len, label_id);

  GtkWidget* entry = gtk_entry_new();
  g_signal_connect(entry, "changed", changed, expander);
  FormTableSetWidget(table, entry, row, col, len, false);

  return entry;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AutoFillDialog
//
// The contents of the AutoFill dialog.  This dialog allows users to add, edit
// and remove AutoFill profiles.
class AutoFillDialog : public PersonalDataManager::Observer {
 public:
  AutoFillDialog(AutoFillDialogObserver* observer,
                 PersonalDataManager* personal_data_manager,
                 AutoFillProfile* imported_profile,
                 CreditCard* imported_credit_card);
  ~AutoFillDialog();

  // PersonalDataManager::Observer implementation:
  void OnPersonalDataLoaded();

  // Shows the AutoFill dialog.
  void Show();

 private:
  // 'destroy' signal handler.  Calls DeleteSoon on the global singleton dialog
  // object.
  static void OnDestroy(GtkWidget* widget, AutoFillDialog* autofill_dialog);

  // 'response' signal handler.  Notifies the AutoFillDialogObserver that new
  // data is available if the response is GTK_RESPONSE_APPLY or GTK_RESPONSE_OK.
  // We close the dialog if the response is GTK_RESPONSE_OK or
  // GTK_RESPONSE_CANCEL.
  static void OnResponse(GtkDialog* dialog, gint response_id,
                         AutoFillDialog* autofill_dialog);

  // 'clicked' signal handler.  Adds a new address.
  static void OnAddAddressClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  Adds a new credit card.
  static void OnAddCreditCardClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  Deletes the associated address.
  static void OnDeleteAddressClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  Deletes the associated credit card.
  static void OnDeleteCreditCardClicked(GtkButton* button,
                                        AutoFillDialog* dialog);

  // 'changed' signal handler.  Updates the title of the expander widget with
  // the contents of the label entry widget.
  static void OnLabelChanged(GtkEntry* label, GtkWidget* expander);

  // Opens the 'Learn more' link in a new foreground tab.
  void OnLinkActivated();

  // Loads AutoFill profiles and credit cards using the PersonalDataManager.
  void LoadAutoFillData();

  // Creates the dialog UI widgets.
  void InitializeWidgets();

  // Initializes the group widgets and returns their container.  |name_id| is
  // the resource ID of the group label.  |button_id| is the resource name of
  // the button label.  |clicked_callback| is a callback that handles the
  // 'clicked' signal emitted when the user presses the 'Add' button.
  GtkWidget* InitGroup(int label_id,
                       int button_id,
                       GCallback clicked_callback);

  // Initializes the expander, frame and table widgets used to hold the address
  // and credit card forms.  |name_id| is the resource id of the label of the
  // expander widget.  The content vbox widget is returned in |content_vbox|.
  // Returns the expander widget.
  GtkWidget* InitGroupContentArea(int name_id, GtkWidget** content_vbox);

  // Returns a GtkExpander that is added to the appropriate vbox.  Each method
  // adds the necessary widgets and layout required to fill out information
  // for either an address or a credit card.  The expander will be expanded by
  // default if |expand| is true.
  GtkWidget* AddNewAddress(bool expand);
  GtkWidget* AddNewCreditCard(bool expand);

  // Adds a new address filled out with information from |profile|.
  void AddAddress(const AutoFillProfile& profile);

  // Adds a new credit card filled out with information from |credit_card|.
  void AddCreditCard(const CreditCard& credit_card);

  // Returns the index of |billing_address| in the list of profiles.  Returns -1
  // if the address is not found.
  int FindIndexOfAddress(string16 billing_address);

  // Our observer.  May not be NULL.
  AutoFillDialogObserver* observer_;

  // The personal data manager, used to load AutoFill profiles and credit cards.
  // Unowned pointer, may not be NULL.
  PersonalDataManager* personal_data_;

  // The imported profile.  May be NULL.
  AutoFillProfile* imported_profile_;

  // The imported credit card.  May be NULL.
  CreditCard* imported_credit_card_;

  // The list of current AutoFill profiles.
  std::vector<AutoFillProfile> profiles_;

  // The list of current AutoFill credit cards.
  std::vector<CreditCard> credit_cards_;

  // The list of address widgets, used to modify the AutoFill profiles.
  std::vector<AddressWidgets> address_widgets_;

  // The list of credit card widgets, used to modify the stored credit cards.
  std::vector<CreditCardWidgets> credit_card_widgets_;

  // The AutoFill dialog.
  GtkWidget* dialog_;

  // The addresses group.
  GtkWidget* addresses_vbox_;

  // The credit cards group.
  GtkWidget* creditcards_vbox_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillDialog);
};

// The singleton AutoFill dialog object.
static AutoFillDialog* dialog = NULL;

AutoFillDialog::AutoFillDialog(AutoFillDialogObserver* observer,
                               PersonalDataManager* personal_data_manager,
                               AutoFillProfile* imported_profile,
                               CreditCard* imported_credit_card)
    : observer_(observer),
      personal_data_(personal_data_manager),
      imported_profile_(imported_profile),
      imported_credit_card_(imported_credit_card) {
  DCHECK(observer_);
  DCHECK(personal_data_);

  InitializeWidgets();
  LoadAutoFillData();

  gtk_util::ShowDialogWithLocalizedSize(dialog_,
                                        IDS_AUTOFILL_DIALOG_WIDTH_CHARS,
                                        IDS_AUTOFILL_DIALOG_HEIGHT_LINES,
                                        true);
}

AutoFillDialog::~AutoFillDialog() {
  // Removes observer if we are observing Profile load. Does nothing otherwise.
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManager::Observer implementation:
void  AutoFillDialog::OnPersonalDataLoaded() {
  personal_data_->RemoveObserver(this);
  LoadAutoFillData();
}

void AutoFillDialog::Show() {
  gtk_util::PresentWindow(dialog_, gtk_get_current_event_time());
}

// static
void AutoFillDialog::OnDestroy(GtkWidget* widget,
                               AutoFillDialog* autofill_dialog) {
  dialog = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, autofill_dialog);
}

static AutoFillProfile AutoFillProfileFromWidgetValues(
    const AddressWidgets& widgets) {
  // TODO(jhawkins): unique id?
  AutoFillProfile profile(GetEntryText(widgets.label), 0);
  profile.SetInfo(AutoFillType(NAME_FULL),
                  GetEntryText(widgets.full_name));
  profile.SetInfo(AutoFillType(EMAIL_ADDRESS),
      GetEntryText(widgets.email));
  profile.SetInfo(AutoFillType(COMPANY_NAME),
      GetEntryText(widgets.company_name));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE1),
      GetEntryText(widgets.address_line1));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2),
      GetEntryText(widgets.address_line2));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_CITY),
      GetEntryText(widgets.city));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_STATE),
      GetEntryText(widgets.state));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_ZIP),
      GetEntryText(widgets.zipcode));
  profile.SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY),
      GetEntryText(widgets.country));

  string16 number, city_code, country_code;
  PhoneNumber::ParsePhoneNumber(
      GetEntryText(widgets.phone), &number, &city_code, &country_code);
  profile.SetInfo(AutoFillType(PHONE_HOME_COUNTRY_CODE), country_code);
  profile.SetInfo(AutoFillType(PHONE_HOME_CITY_CODE), city_code);
  profile.SetInfo(AutoFillType(PHONE_HOME_NUMBER), number);

  PhoneNumber::ParsePhoneNumber(
      GetEntryText(widgets.fax), &number, &city_code, &country_code);
  profile.SetInfo(AutoFillType(PHONE_FAX_COUNTRY_CODE), country_code);
  profile.SetInfo(AutoFillType(PHONE_FAX_CITY_CODE), city_code);
  profile.SetInfo(AutoFillType(PHONE_FAX_NUMBER), number);

  return profile;
}

static CreditCard CreditCardFromWidgetValues(
    const CreditCardWidgets& widgets) {
  // TODO(jhawkins): unique id?
  CreditCard credit_card(GetEntryText(widgets.label), 0);
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME),
                      GetEntryText(widgets.name_on_card));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH),
                      GetEntryText(widgets.expiration_month));
  credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                      GetEntryText(widgets.expiration_year));

  // If the CC number starts with an asterisk, then we know that the user has
  // not modified the credit card number at the least, so use the original CC
  // number in this case.
  string16 cc_number = GetEntryText(widgets.card_number);
  if (!cc_number.empty() && cc_number[0] == '*')
    credit_card.SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
                        widgets.original_card_number);
  else
    credit_card.SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
                        GetEntryText(widgets.card_number));

  std::string text =
      gtk_combo_box_get_active_text(GTK_COMBO_BOX(widgets.billing_address));
  if (text !=
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_CHOOSE_EXISTING_ADDRESS)) {
    // TODO(jhawkins): Should we validate the billing address combobox?
    credit_card.set_billing_address(UTF8ToUTF16(text));
  }

  return credit_card;
}

// static
void AutoFillDialog::OnResponse(GtkDialog* dialog, gint response_id,
                                AutoFillDialog* autofill_dialog) {
  if (response_id == GTK_RESPONSE_APPLY || response_id == GTK_RESPONSE_OK) {
    autofill_dialog->profiles_.clear();
    for (std::vector<AddressWidgets>::const_iterator iter =
             autofill_dialog->address_widgets_.begin();
         iter != autofill_dialog->address_widgets_.end();
         ++iter) {
      AutoFillProfile profile = AutoFillProfileFromWidgetValues(*iter);
      autofill_dialog->profiles_.push_back(profile);
    }

    autofill_dialog->credit_cards_.clear();
    for (std::vector<CreditCardWidgets>::const_iterator iter =
             autofill_dialog->credit_card_widgets_.begin();
         iter != autofill_dialog->credit_card_widgets_.end();
         ++iter) {
      CreditCard credit_card = CreditCardFromWidgetValues(*iter);
      autofill_dialog->credit_cards_.push_back(credit_card);
    }

    autofill_dialog->observer_->OnAutoFillDialogApply(
        &autofill_dialog->profiles_,
        &autofill_dialog->credit_cards_);
  }

  if (response_id == GTK_RESPONSE_OK ||
      response_id == GTK_RESPONSE_CANCEL ||
      response_id == GTK_RESPONSE_DELETE_EVENT) {
    gtk_widget_destroy(GTK_WIDGET(dialog));
  }

  if (response_id == kAutoFillDialogLearnMoreLink)
    autofill_dialog->OnLinkActivated();
}

// static
void AutoFillDialog::OnAddAddressClicked(GtkButton* button,
                                         AutoFillDialog* dialog) {
  GtkWidget* new_address = dialog->AddNewAddress(true);
  gtk_box_pack_start(GTK_BOX(dialog->addresses_vbox_), new_address,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(new_address);
}

// static
void AutoFillDialog::OnAddCreditCardClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  GtkWidget* new_creditcard = dialog->AddNewCreditCard(true);
  gtk_box_pack_start(GTK_BOX(dialog->creditcards_vbox_), new_creditcard,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(new_creditcard);
}

// static
void AutoFillDialog::OnDeleteAddressClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  GtkWidget* entry = GetButtonData(GTK_WIDGET(button));
  string16 label = GetEntryText(entry);

  // TODO(jhawkins): Base this on ID.

  // Remove the profile.
  for (std::vector<AutoFillProfile>::iterator iter = dialog->profiles_.begin();
       iter != dialog->profiles_.end();
       ++iter) {
    if (iter->Label() == label) {
      dialog->profiles_.erase(iter);
      break;
    }
  }

  // Remove the set of address widgets.
  for (std::vector<AddressWidgets>::iterator iter =
           dialog->address_widgets_.begin();
       iter != dialog->address_widgets_.end();
       ++iter) {
    if (iter->label == entry) {
      dialog->address_widgets_.erase(iter);
      break;
    }
  }

  // Get back to the expander widget.
  GtkWidget* expander = gtk_widget_get_ancestor(GTK_WIDGET(button),
                                                GTK_TYPE_EXPANDER);
  DCHECK(expander);

  // Destroying the widget will also remove it from the parent container.
  gtk_widget_destroy(expander);
}

// static
void AutoFillDialog::OnDeleteCreditCardClicked(GtkButton* button,
                                               AutoFillDialog* dialog) {
  GtkWidget* entry = GetButtonData(GTK_WIDGET(button));
  string16 label = GetEntryText(entry);

  // TODO(jhawkins): Base this on ID.

  // Remove the credit card.
  for (std::vector<CreditCard>::iterator iter = dialog->credit_cards_.begin();
       iter != dialog->credit_cards_.end();
       ++iter) {
    if (iter->Label() == label) {
      dialog->credit_cards_.erase(iter);
      break;
    }
  }

  // Remove the set of credit widgets.
  for (std::vector<CreditCardWidgets>::iterator iter =
           dialog->credit_card_widgets_.begin();
       iter != dialog->credit_card_widgets_.end();
       ++iter) {
    if (iter->label == entry) {
      dialog->credit_card_widgets_.erase(iter);
      break;
    }
  }

  // Get back to the expander widget.
  GtkWidget* expander = gtk_widget_get_ancestor(GTK_WIDGET(button),
                                                GTK_TYPE_EXPANDER);
  DCHECK(expander);

  // Destroying the widget will also remove it from the parent container.
  gtk_widget_destroy(expander);
}

// static
void AutoFillDialog::OnLabelChanged(GtkEntry* label, GtkWidget* expander) {
  gtk_expander_set_label(GTK_EXPANDER(expander), gtk_entry_get_text(label));
}

void AutoFillDialog::OnLinkActivated() {
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(GURL(kAutoFillLearnMoreUrl), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::TYPED);
}

void AutoFillDialog::LoadAutoFillData() {
  if (!personal_data_->IsDataLoaded()) {
    personal_data_->SetObserver(this);
    return;
  }

  if (imported_profile_) {
    profiles_.push_back(*imported_profile_);
    AddAddress(*imported_profile_);
  } else {
    for (std::vector<AutoFillProfile*>::const_iterator iter =
             personal_data_->profiles().begin();
         iter != personal_data_->profiles().end(); ++iter) {
      // The profile list is terminated by a NULL entry;
      if (!*iter)
        break;

      AutoFillProfile* profile = *iter;
      profiles_.push_back(*profile);
      AddAddress(*profile);
    }
  }

  if (imported_credit_card_) {
    credit_cards_.push_back(*imported_credit_card_);
    AddCreditCard(*imported_credit_card_);
  } else {
    for (std::vector<CreditCard*>::const_iterator iter =
             personal_data_->credit_cards().begin();
         iter != personal_data_->credit_cards().end(); ++iter) {
      // The credit card list is terminated by a NULL entry;
      if (!*iter)
        break;

      CreditCard* credit_card = *iter;
      credit_cards_.push_back(*credit_card);
      AddCreditCard(*credit_card);
    }
  }
}

void AutoFillDialog::InitializeWidgets() {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_OPTIONS).c_str(),
      // AutoFill dialog is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_APPLY,
      GTK_RESPONSE_APPLY,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      NULL);

  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);

  // Allow the contents to be scrolled.
  GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), scrolled_window);

  // We create an event box so that we can color the frame background white.
  GtkWidget* frame_event_box = gtk_event_box_new();
  SetWhiteBackground(frame_event_box);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
                                        frame_event_box);

  // The frame outline of the content area.
  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_container_add(GTK_CONTAINER(frame_event_box), frame);

  // The content vbox.
  GtkWidget* outer_vbox = gtk_vbox_new(false, 0);
  gtk_box_set_spacing(GTK_BOX(outer_vbox), gtk_util::kContentAreaSpacing);
  gtk_container_add(GTK_CONTAINER(frame), outer_vbox);

  addresses_vbox_ = InitGroup(IDS_AUTOFILL_ADDRESSES_GROUP_NAME,
                              IDS_AUTOFILL_ADD_ADDRESS_BUTTON,
                              G_CALLBACK(OnAddAddressClicked));
  gtk_box_pack_start_defaults(GTK_BOX(outer_vbox), addresses_vbox_);

  creditcards_vbox_ = InitGroup(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME,
                                IDS_AUTOFILL_ADD_CREDITCARD_BUTTON,
                                G_CALLBACK(OnAddCreditCardClicked));
  gtk_box_pack_start_defaults(GTK_BOX(outer_vbox), creditcards_vbox_);

  GtkWidget* link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_LEARN_MORE).c_str());
  gtk_dialog_add_action_widget(GTK_DIALOG(dialog_), link,
                               kAutoFillDialogLearnMoreLink);

  // Setting the link widget to secondary positions the button on the left side
  // of the action area (vice versa for RTL layout).
  gtk_button_box_set_child_secondary(
      GTK_BUTTON_BOX(GTK_DIALOG(dialog_)->action_area), link, TRUE);
}

GtkWidget* AutoFillDialog::InitGroup(int name_id,
                                     int button_id,
                                     GCallback clicked_callback) {
  GtkWidget* vbox = gtk_vbox_new(false, gtk_util::kControlSpacing);

  // Group label.
  GtkWidget* label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(name_id));
  gtk_box_pack_start(GTK_BOX(vbox),
                     IndentWidget(label, kAutoFillDialogIndent),
                     FALSE, FALSE, 0);

  // Separator.
  GtkWidget* separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

  // Add profile button.
  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(button_id).c_str());
  g_signal_connect(button, "clicked", clicked_callback, this);
  gtk_box_pack_end_defaults(GTK_BOX(vbox),
                            IndentWidget(button, kAutoFillDialogIndent));

  return vbox;
}

GtkWidget* AutoFillDialog::InitGroupContentArea(int name_id,
                                                GtkWidget** content_vbox) {
  GtkWidget* expander = gtk_expander_new(
      l10n_util::GetStringUTF8(name_id).c_str());

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_container_add(GTK_CONTAINER(expander), frame);

  GtkWidget* vbox = gtk_vbox_new(false, 0);
  gtk_box_set_spacing(GTK_BOX(vbox), gtk_util::kControlSpacing);
  GtkWidget* vbox_alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(vbox_alignment),
                            gtk_util::kControlSpacing,
                            gtk_util::kControlSpacing,
                            gtk_util::kGroupIndent,
                            0);
  gtk_container_add(GTK_CONTAINER(vbox_alignment), vbox);
  gtk_container_add(GTK_CONTAINER(frame), vbox_alignment);

  *content_vbox = vbox;
  return expander;
}

GtkWidget* AutoFillDialog::AddNewAddress(bool expand) {
  AddressWidgets widgets = {0};
  GtkWidget* vbox;
  GtkWidget* address = InitGroupContentArea(IDS_AUTOFILL_NEW_ADDRESS, &vbox);

  gtk_expander_set_expanded(GTK_EXPANDER(address), expand);

  GtkWidget* table = InitFormTable(5, 3);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), table);

  widgets.label = FormTableAddLabelEntry(table, 0, 0, 1,
                                         IDS_AUTOFILL_DIALOG_LABEL,
                                         address, G_CALLBACK(OnLabelChanged));
  widgets.full_name = FormTableAddEntry(table, 1, 0, 1,
                                        IDS_AUTOFILL_DIALOG_FULL_NAME);
  widgets.email = FormTableAddEntry(table, 2, 0, 1,
                                    IDS_AUTOFILL_DIALOG_EMAIL);
  widgets.company_name = FormTableAddEntry(table, 2, 1, 1,
                                           IDS_AUTOFILL_DIALOG_COMPANY_NAME);
  widgets.address_line1 = FormTableAddEntry(table, 3, 0, 2,
                                            IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1);
  widgets.address_line2 = FormTableAddEntry(table, 4, 0, 2,
                                            IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2);

  GtkWidget* address_table = InitFormTable(1, 4);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), address_table);

  widgets.city = FormTableAddEntry(address_table, 0, 0, 1,
                                   IDS_AUTOFILL_DIALOG_CITY);
  widgets.state = FormTableAddEntry(address_table, 0, 1, 1,
                                    IDS_AUTOFILL_DIALOG_STATE);
  widgets.zipcode = FormTableAddSizedEntry(address_table, 0, 2, 7,
                                           IDS_AUTOFILL_DIALOG_ZIP_CODE);
  widgets.country = FormTableAddSizedEntry(address_table, 0, 3, 10,
                                           IDS_AUTOFILL_DIALOG_COUNTRY);

  GtkWidget* phone_table = InitFormTable(1, 2);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), phone_table);

  widgets.phone =
      FormTableAddEntry(phone_table, 0, 0, 1, IDS_AUTOFILL_DIALOG_PHONE);
  widgets.fax =
      FormTableAddEntry(phone_table, 0, 1, 1, IDS_AUTOFILL_DIALOG_FAX);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  g_signal_connect(button, "clicked", G_CALLBACK(OnDeleteAddressClicked), this);
  SetButtonData(button, widgets.label);
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), button);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), alignment);

  address_widgets_.push_back(widgets);
  return address;
}

GtkWidget* AutoFillDialog::AddNewCreditCard(bool expand) {
  CreditCardWidgets widgets = {0};
  GtkWidget* vbox;
  GtkWidget* credit_card = InitGroupContentArea(IDS_AUTOFILL_NEW_CREDITCARD,
                                                &vbox);

  gtk_expander_set_expanded(GTK_EXPANDER(credit_card), expand);

  GtkWidget* label_table = InitFormTable(1, 2);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), label_table);

  widgets.label = FormTableAddLabelEntry(label_table, 0, 0, 1,
                                         IDS_AUTOFILL_DIALOG_LABEL, credit_card,
                                         G_CALLBACK(OnLabelChanged));

  GtkWidget* name_cc_table = InitFormTable(2, 6);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), name_cc_table);

  widgets.name_on_card = FormTableAddExpandedEntry(
      name_cc_table, 0, 0, 3, IDS_AUTOFILL_DIALOG_NAME_ON_CARD);
  widgets.card_number = FormTableAddExpandedEntry(
      name_cc_table, 1, 0, 3, IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER);
  widgets.expiration_month = FormTableAddSizedEntry(name_cc_table, 1, 3, 2, 0);
  widgets.expiration_year = FormTableAddSizedEntry(name_cc_table, 1, 4, 4, 0);

  FormTableSetLabel(name_cc_table, 1, 3, 2,
                    IDS_AUTOFILL_DIALOG_EXPIRATION_DATE);

  gtk_table_set_col_spacing(GTK_TABLE(name_cc_table), 3, 2);

  GtkWidget* addresses_table = InitFormTable(2, 5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), addresses_table);

  FormTableSetLabel(addresses_table, 0, 0, 3,
                    IDS_AUTOFILL_DIALOG_BILLING_ADDRESS);

  GtkWidget* billing = gtk_combo_box_new_text();
  widgets.billing_address = billing;
  std::string combo_text = l10n_util::GetStringUTF8(
      IDS_AUTOFILL_DIALOG_CHOOSE_EXISTING_ADDRESS);
  gtk_combo_box_append_text(GTK_COMBO_BOX(billing), combo_text.c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(billing), 0);
  FormTableSetWidget(addresses_table, billing, 0, 0, 2, false);

  for (std::vector<AddressWidgets>::const_iterator iter =
           address_widgets_.begin();
       iter != address_widgets_.end(); ++iter) {
    // TODO(jhawkins): Validate the label and DCHECK on !empty().
    std::string text = gtk_entry_get_text(GTK_ENTRY(iter->label));
    if (!text.empty())
      gtk_combo_box_append_text(GTK_COMBO_BOX(widgets.billing_address),
                                text.c_str());
  }

  GtkWidget* phone_table = InitFormTable(1, 1);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), phone_table);

  widgets.phone =
      FormTableAddEntry(phone_table, 0, 0, 1, IDS_AUTOFILL_DIALOG_PHONE);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  g_signal_connect(button, "clicked",
                   G_CALLBACK(OnDeleteCreditCardClicked), this);
  SetButtonData(button, widgets.label);
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), button);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), alignment);

  credit_card_widgets_.push_back(widgets);
  return credit_card;
}

void AutoFillDialog::AddAddress(const AutoFillProfile& profile) {
  GtkWidget* address = AddNewAddress(false);
  gtk_expander_set_label(GTK_EXPANDER(address),
                         UTF16ToUTF8(profile.Label()).c_str());

  // We just pushed the widgets to the back of the vector.
  const AddressWidgets& widgets = address_widgets_.back();
  SetEntryText(widgets.label, profile.Label());
  SetEntryText(widgets.full_name,
               profile.GetFieldText(AutoFillType(NAME_FULL)));
  SetEntryText(widgets.email,
               profile.GetFieldText(AutoFillType(EMAIL_ADDRESS)));
  SetEntryText(widgets.company_name,
               profile.GetFieldText(AutoFillType(COMPANY_NAME)));
  SetEntryText(widgets.address_line1,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)));
  SetEntryText(widgets.address_line2,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)));
  SetEntryText(widgets.city,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_CITY)));
  SetEntryText(widgets.state,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_STATE)));
  SetEntryText(widgets.zipcode,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_ZIP)));
  SetEntryText(widgets.country,
               profile.GetFieldText(AutoFillType(ADDRESS_HOME_COUNTRY)));
  SetEntryText(widgets.phone,
               profile.GetFieldText(AutoFillType(PHONE_HOME_WHOLE_NUMBER)));
  SetEntryText(widgets.fax,
               profile.GetFieldText(AutoFillType(PHONE_FAX_WHOLE_NUMBER)));

  gtk_box_pack_start(GTK_BOX(addresses_vbox_), address, FALSE, FALSE, 0);
  gtk_widget_show_all(address);
}

void AutoFillDialog::AddCreditCard(const CreditCard& credit_card) {
  GtkWidget* credit_card_widget = AddNewCreditCard(false);
  gtk_expander_set_label(GTK_EXPANDER(credit_card_widget),
                         UTF16ToUTF8(credit_card.Label()).c_str());

  // We just pushed the widgets to the back of the vector.
  const CreditCardWidgets& widgets = credit_card_widgets_.back();
  SetEntryText(widgets.label, credit_card.Label());
  SetEntryText(widgets.name_on_card,
               credit_card.GetFieldText(AutoFillType(CREDIT_CARD_NAME)));
  // Set obfuscated number if not empty.
  credit_card_widgets_.back().original_card_number =
      credit_card.GetFieldText(AutoFillType(CREDIT_CARD_NUMBER));
  string16 credit_card_number;
  if (!widgets.original_card_number.empty())
    credit_card_number = credit_card.ObfuscatedNumber();
  SetEntryText(widgets.card_number, credit_card_number);
  SetEntryText(widgets.expiration_month,
               credit_card.GetFieldText(AutoFillType(CREDIT_CARD_EXP_MONTH)));
  SetEntryText(
      widgets.expiration_year,
      credit_card.GetFieldText(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)));

  // Two cases to consider:
  //  address not found - This means the address is not set and
  //    FindIndexOfAddress returns -1.  -1 + 1 = 0, meaning the first item will
  //    be selected, "Choose existing address".
  //
  //  address is found - The index returned needs to be offset by one in order
  //    to compensate for the first entry, "Choose existing address".
  int index = FindIndexOfAddress(credit_card.billing_address()) + 1;
  gtk_combo_box_set_active(GTK_COMBO_BOX(widgets.billing_address), index);

  gtk_box_pack_start(GTK_BOX(creditcards_vbox_), credit_card_widget,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(credit_card_widget);
}

int AutoFillDialog::FindIndexOfAddress(string16 billing_address) {
  int index = 0;
  for (std::vector<AddressWidgets>::const_iterator iter =
           address_widgets_.begin();
       iter != address_widgets_.end(); ++iter, ++index) {
    std::string text = gtk_entry_get_text(GTK_ENTRY(iter->label));
    if (UTF8ToUTF16(text) == billing_address)
      return index;
  }

  return -1;
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowAutoFillDialog(gfx::NativeView parent,
                        AutoFillDialogObserver* observer,
                        Profile* profile,
                        AutoFillProfile* imported_profile,
                        CreditCard* imported_credit_card) {
  DCHECK(profile);

  if (!dialog) {
    dialog = new AutoFillDialog(observer,
                                profile->GetPersonalDataManager(),
                                imported_profile,
                                imported_credit_card);
  }
  dialog->Show();
}
