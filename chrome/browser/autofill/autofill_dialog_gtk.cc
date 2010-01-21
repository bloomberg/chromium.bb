// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

#include <gtk/gtk.h>

#include <vector>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Style for dialog group titles.
const char kDialogGroupTitleMarkup[] = "<span weight='bold'>%s</span>";

// How far we indent dialog widgets, in pixels.
const int kAutoFillDialogIndent = 5;

// All of these widgets are GtkEntrys except for default_profile, which is a
// GtkCheckButton.
typedef struct _AddressWidgets {
  GtkWidget* label;
  GtkWidget* default_profile;
  GtkWidget* first_name;
  GtkWidget* middle_name;
  GtkWidget* last_name;
  GtkWidget* email;
  GtkWidget* company_name;
  GtkWidget* address_line1;
  GtkWidget* address_line2;
  GtkWidget* city;
  GtkWidget* state;
  GtkWidget* zipcode;
  GtkWidget* country;
  GtkWidget* phone1;
  GtkWidget* phone2;
  GtkWidget* phone3;
  GtkWidget* fax1;
  GtkWidget* fax2;
  GtkWidget* fax3;
} AddressWidgets;

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

  const char* text =
      (label_id) ? l10n_util::GetStringUTF8(label_id).c_str() : 0;
  GtkWidget* label = gtk_label_new(text);
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
class AutoFillDialog {
 public:
  AutoFillDialog(AutoFillDialogObserver* observer,
                 const std::vector<AutoFillProfile>& profiles,
                 const std::vector<CreditCard>& credit_cards);
  ~AutoFillDialog() {}

  // Shows the AutoFill dialog.
  void Show();

 private:
  // 'destroy' signal handler.  We DeleteSoon the global singleton dialog object
  // from here.
  static void OnDestroy(GtkWidget* widget, AutoFillDialog* autofill_dialog);

  // 'response' signal handler.  We notify the AutoFillDialogObserver that new
  // data is available if the response is GTK_RESPONSE_APPLY or GTK_RESPONSE_OK.
  // We close the dialog if the response is GTK_RESPONSE_OK or
  // GTK_RESPONSE_CANCEL.
  static void OnResponse(GtkDialog* dialog, gint response_id,
                         AutoFillDialog* autofill_dialog);

  // 'clicked' signal handler.  We add a new address.
  static void OnAddAddressClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  We add a new credit card.
  static void OnAddCreditCardClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'changed' signal handler.  We update the title of the expander widget with
  // the contents of the label entry widget.
  static void OnLabelChanged(GtkEntry* label, GtkWidget* expander);

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
  // for either an address or a credit card.
  GtkWidget* AddNewAddress();
  GtkWidget* AddNewCreditCard();

  // The list of current AutoFill profiles.
  std::vector<AutoFillProfile> profiles_;

  // The list of current AutoFill credit cards.
  std::vector<CreditCard> credit_cards_;

  // The list of address widgets, used to modify the AutoFill profiles.
  std::vector<AddressWidgets> address_widgets_;

  // The AutoFill dialog.
  GtkWidget* dialog_;

  // The addresses group.
  GtkWidget* addresses_vbox_;

  // The credit cards group.
  GtkWidget* creditcards_vbox_;

  // Our observer.
  AutoFillDialogObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillDialog);
};

// The singleton AutoFill dialog object.
static AutoFillDialog* dialog = NULL;

AutoFillDialog::AutoFillDialog(AutoFillDialogObserver* observer,
                               const std::vector<AutoFillProfile>& profiles,
                               const std::vector<CreditCard>& credit_cards)
    : profiles_(profiles),
      credit_cards_(credit_cards),
      observer_(observer) {
  DCHECK(observer);

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_TITLE).c_str(),
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

  gtk_widget_realize(dialog_);
  gtk_util::SetWindowSizeFromResources(GTK_WINDOW(dialog_),
                                       IDS_AUTOFILL_DIALOG_WIDTH_CHARS,
                                       IDS_AUTOFILL_DIALOG_HEIGHT_LINES,
                                       true);

  // Allow browser windows to go in front of the AutoFill dialog in Metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
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

  // TODO(jhawkins): Add addresses from |profiles|.

  creditcards_vbox_ = InitGroup(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME,
                                IDS_AUTOFILL_ADD_CREDITCARD_BUTTON,
                                G_CALLBACK(OnAddCreditCardClicked));
  gtk_box_pack_start_defaults(GTK_BOX(outer_vbox), creditcards_vbox_);

  // TODO(jhawkins): Add credit cards from |credit_cards|.

  gtk_widget_show_all(dialog_);
}

void AutoFillDialog::Show() {
  gtk_window_present_with_time(GTK_WINDOW(dialog_),
                               gtk_get_current_event_time());
}

// static
void AutoFillDialog::OnDestroy(GtkWidget* widget,
                               AutoFillDialog* autofill_dialog) {
  dialog = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, autofill_dialog);
}

static string16 GetEntryText(GtkWidget* entry) {
  return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(entry)));
}

static AutoFillProfile AutoFillProfileFromWidgetValues(
    const AddressWidgets& widgets) {
  // TODO(jhawkins): unique id?
  AutoFillProfile profile(GetEntryText(widgets.label), 0);
  profile.SetInfo(AutoFillType(NAME_FIRST),
                  GetEntryText(widgets.first_name));
  profile.SetInfo(AutoFillType(NAME_MIDDLE),
      GetEntryText(widgets.middle_name));
  profile.SetInfo(AutoFillType(NAME_LAST),
      GetEntryText(widgets.last_name));
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
  profile.SetInfo(AutoFillType(PHONE_HOME_COUNTRY_CODE),
      GetEntryText(widgets.phone1));
  profile.SetInfo(AutoFillType(PHONE_HOME_CITY_CODE),
      GetEntryText(widgets.phone2));
  profile.SetInfo(AutoFillType(PHONE_HOME_NUMBER),
      GetEntryText(widgets.phone3));
  profile.SetInfo(AutoFillType(PHONE_FAX_COUNTRY_CODE),
      GetEntryText(widgets.fax1));
  profile.SetInfo(AutoFillType(PHONE_FAX_CITY_CODE),
      GetEntryText(widgets.fax2));
  profile.SetInfo(AutoFillType(PHONE_FAX_NUMBER),
      GetEntryText(widgets.fax3));
  return profile;
}

// static
void AutoFillDialog::OnResponse(GtkDialog* dialog, gint response_id,
                                AutoFillDialog* autofill_dialog) {
  if (response_id == GTK_RESPONSE_APPLY || response_id == GTK_RESPONSE_OK) {
    autofill_dialog->profiles_.clear();
    std::vector<AddressWidgets>::const_iterator iter;
    for (iter = autofill_dialog->address_widgets_.begin();
         iter != autofill_dialog->address_widgets_.end();
         ++iter) {
      autofill_dialog->profiles_.push_back(
          AutoFillProfileFromWidgetValues(*iter));
    }

    autofill_dialog->observer_->OnAutoFillDialogApply(
        autofill_dialog->profiles_, autofill_dialog->credit_cards_);
  }

  if (response_id == GTK_RESPONSE_OK || response_id == GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy(GTK_WIDGET(dialog));
  }
}

// static
void AutoFillDialog::OnAddAddressClicked(GtkButton* button,
                                         AutoFillDialog* dialog) {
  GtkWidget* new_address = dialog->AddNewAddress();
  gtk_box_pack_start(GTK_BOX(dialog->addresses_vbox_), new_address,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(new_address);
}

// static
void AutoFillDialog::OnAddCreditCardClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  GtkWidget* new_creditcard = dialog->AddNewCreditCard();
  gtk_box_pack_start(GTK_BOX(dialog->creditcards_vbox_), new_creditcard,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(new_creditcard);
}

// static
void AutoFillDialog::OnLabelChanged(GtkEntry* label, GtkWidget* expander) {
  gtk_expander_set_label(GTK_EXPANDER(expander), gtk_entry_get_text(label));
}

GtkWidget* AutoFillDialog::InitGroup(int name_id,
                                     int button_id,
                                     GCallback clicked_callback) {
  GtkWidget* vbox = gtk_vbox_new(false, gtk_util::kControlSpacing);

  // Group label.
  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(
      kDialogGroupTitleMarkup,
      l10n_util::GetStringUTF8(name_id).c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
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

  // Make it expand by default.
  gtk_expander_set_expanded(GTK_EXPANDER(expander), true);

  *content_vbox = vbox;
  return expander;
}

GtkWidget* AutoFillDialog::AddNewAddress() {
  AddressWidgets widgets = {0};
  GtkWidget* vbox;
  GtkWidget* address = InitGroupContentArea(IDS_AUTOFILL_NEW_ADDRESS, &vbox);

  GtkWidget* table = InitFormTable(5, 3);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), table);

  widgets.label = FormTableAddLabelEntry(table, 0, 0, 1,
                                         IDS_AUTOFILL_DIALOG_LABEL,
                                         address, G_CALLBACK(OnLabelChanged));
  widgets.first_name = FormTableAddEntry(table, 1, 0, 1,
                                         IDS_AUTOFILL_DIALOG_FIRST_NAME);
  widgets.middle_name = FormTableAddEntry(table, 1, 1, 1,
                                          IDS_AUTOFILL_DIALOG_MIDDLE_NAME);
  widgets.last_name = FormTableAddEntry(table, 1, 2, 1,
                                        IDS_AUTOFILL_DIALOG_LAST_NAME);
  widgets.email = FormTableAddEntry(table, 2, 0, 1,
                                    IDS_AUTOFILL_DIALOG_EMAIL);
  widgets.company_name = FormTableAddEntry(table, 2, 1, 1,
                                           IDS_AUTOFILL_DIALOG_COMPANY_NAME);
  widgets.address_line1 = FormTableAddEntry(table, 3, 0, 2,
                                            IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1);
  widgets.address_line2 = FormTableAddEntry(table, 4, 0, 2,
                                            IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2);

  // TODO(jhawkins): If there's not a default profile, automatically check this
  // check button.
  GtkWidget* default_check = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_MAKE_DEFAULT).c_str());
  widgets.default_profile = default_check;
  FormTableSetWidget(table, default_check, 0, 1, 1, false);

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

  GtkWidget* phone_table = InitFormTable(1, 8);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), phone_table);

  widgets.phone1 = FormTableAddSizedEntry(phone_table, 0, 0, 4,
                                          IDS_AUTOFILL_DIALOG_PHONE);
  widgets.phone2 = FormTableAddSizedEntry(phone_table, 0, 1, 4, 0);
  widgets.phone3 = FormTableAddEntry(phone_table, 0, 2, 2, 0);
  widgets.fax1 = FormTableAddSizedEntry(phone_table, 0, 4, 4,
                                        IDS_AUTOFILL_DIALOG_FAX);
  widgets.fax2 = FormTableAddSizedEntry(phone_table, 0, 5, 4, 0);
  widgets.fax3 = FormTableAddEntry(phone_table, 0, 6, 2, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), button);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), alignment);

  address_widgets_.push_back(widgets);
  return address;
}

GtkWidget* AutoFillDialog::AddNewCreditCard() {
  GtkWidget* vbox;
  GtkWidget* credit_card = InitGroupContentArea(IDS_AUTOFILL_NEW_CREDITCARD,
                                                &vbox);

  GtkWidget* label_table = InitFormTable(1, 2);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), label_table);

  FormTableAddLabelEntry(label_table, 0, 0, 1, IDS_AUTOFILL_DIALOG_LABEL,
                         credit_card, G_CALLBACK(OnLabelChanged));

  // TODO(jhawkins): If there's not a default profile, automatically check this
  // check button.
  GtkWidget* default_check = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_MAKE_DEFAULT).c_str());
  FormTableSetWidget(label_table, default_check, 0, 1, 1, true);

  GtkWidget* name_cc_table = InitFormTable(2, 6);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), name_cc_table);

  FormTableAddExpandedEntry(name_cc_table, 0, 0, 3,
                            IDS_AUTOFILL_DIALOG_NAME_ON_CARD);
  FormTableAddExpandedEntry(name_cc_table, 1, 0, 3,
                            IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER);
  FormTableAddSizedEntry(name_cc_table, 1, 3, 2, 0);
  FormTableAddSizedEntry(name_cc_table, 1, 4, 4, 0);
  FormTableAddSizedEntry(name_cc_table, 1, 5, 5, IDS_AUTOFILL_DIALOG_CVC);

  FormTableSetLabel(name_cc_table, 1, 3, 2,
                    IDS_AUTOFILL_DIALOG_EXPIRATION_DATE);

  gtk_table_set_col_spacing(GTK_TABLE(name_cc_table), 3, 2);

  GtkWidget* addresses_table = InitFormTable(2, 5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), addresses_table);

  FormTableSetLabel(addresses_table, 0, 0, 3,
                    IDS_AUTOFILL_DIALOG_BILLING_ADDRESS);

  GtkWidget* billing = gtk_combo_box_new_text();
  std::string combo_text = l10n_util::GetStringUTF8(
      IDS_AUTOFILL_DIALOG_CHOOSE_EXISTING_ADDRESS);
  gtk_combo_box_append_text(GTK_COMBO_BOX(billing), combo_text.c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(billing), 0);
  FormTableSetWidget(addresses_table, billing, 0, 0, 2, false);

  FormTableSetLabel(addresses_table, 1, 0, 3,
                    IDS_AUTOFILL_DIALOG_SHIPPING_ADDRESS);

  GtkWidget* shipping = gtk_combo_box_new_text();
  combo_text = l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_SAME_AS_BILLING);
  gtk_combo_box_append_text(GTK_COMBO_BOX(shipping), combo_text.c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(shipping), 0);
  FormTableSetWidget(addresses_table, shipping, 1, 0, 2, false);

  GtkWidget* phone_table = InitFormTable(1, 4);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), phone_table);

  FormTableAddSizedEntry(phone_table, 0, 0, 4, IDS_AUTOFILL_DIALOG_PHONE);
  FormTableAddSizedEntry(phone_table, 0, 1, 4, 0);
  FormTableAddEntry(phone_table, 0, 2, 2, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), button);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), alignment);

  return credit_card;
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowAutoFillDialog(AutoFillDialogObserver* observer,
                        const std::vector<AutoFillProfile>& profiles,
                        const std::vector<CreditCard>& credit_cards) {
  if (!dialog) {
    dialog = new AutoFillDialog(observer, profiles, credit_cards);
  }
  dialog->Show();
}
