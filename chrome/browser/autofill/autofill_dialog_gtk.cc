// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

#include <gtk/gtk.h>

#include <vector>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/autofill/autofill_profile.h"
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

// Adds an alignment around |widget| which indents the widget by
// |kAutoFillDialogIndent|.
GtkWidget* IndentWidget(GtkWidget* widget) {
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0,
                            kAutoFillDialogIndent, 0);
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

}  // namespace

// The contents of the AutoFill dialog.  This dialog allows users to add, edit
// and remove AutoFill profiles.
class AutoFillDialog {
 public:
  AutoFillDialog(std::vector<AutoFillProfile>* profiles,
                 std::vector<FormGroup>* credit_cards);
  ~AutoFillDialog() {}

  // Shows the AutoFill dialog.
  void Show();

 private:
  // 'destroy' signal handler.  We DeleteSoon the global singleton dialog object
  // from here.
  static void OnDestroy(GtkWidget* widget, AutoFillDialog* autofill_dialog);

  // 'clicked' signal handler.  We add a new address.
  static void OnAddAddressClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  We add a new credit card.
  static void OnAddCreditCardClicked(GtkButton* button, AutoFillDialog* dialog);

  // Initializes the group widgets and returns their container.  |name_id| is
  // the resource ID of the group label.  |button_id| is the resource name of
  // the button label.  |clicked_callback| is a callback that handles the
  // 'clicked' signal emitted when the user presses the 'Add' button.
  GtkWidget* InitGroup(int label_id,
                       int button_id,
                       GCallback clicked_callback);

  // Returns a GtkExpander that is added to the appropriate vbox.  Each method
  // adds the necessary widgets and layout required to fill out information
  // for either an address or a credit card.
  GtkWidget* AddNewAddress();
  GtkWidget* AddNewCreditCard();

  // The list of current AutoFill profiles.  Owned by AutoFillManager.
  std::vector<AutoFillProfile>* profiles_;

  // The list of current AutoFill credit cards.  Owned by AutoFillManager.
  std::vector<FormGroup>* credit_cards_;

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

AutoFillDialog::AutoFillDialog(std::vector<AutoFillProfile>* profiles,
                               std::vector<FormGroup>* credit_cards)
    : profiles_(profiles),
      credit_cards_(credit_cards) {
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
  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);
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
  GtkWidget* outer_vbox = gtk_vbox_new(true, 0);
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

// static
void AutoFillDialog::OnAddAddressClicked(GtkButton* button,
                                         AutoFillDialog* dialog) {
  GtkWidget* new_address = dialog->AddNewAddress();
  gtk_box_pack_start(GTK_BOX(dialog->addresses_vbox_), new_address,
                     FALSE, FALSE, 0);
  gtk_widget_show(new_address);
}

// static
void AutoFillDialog::OnAddCreditCardClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  GtkWidget* new_creditcard = dialog->AddNewCreditCard();
  gtk_box_pack_start(GTK_BOX(dialog->creditcards_vbox_), new_creditcard,
                     FALSE, FALSE, 0);
  gtk_widget_show(new_creditcard);
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
  gtk_box_pack_start(GTK_BOX(vbox), IndentWidget(label), FALSE, FALSE, 0);

  // Separator.
  GtkWidget* separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

  // Add profile button.
  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(name_id).c_str());
  g_signal_connect(button, "clicked", clicked_callback, this);
  gtk_box_pack_end_defaults(GTK_BOX(vbox), IndentWidget(button));

  return vbox;
}

GtkWidget* AutoFillDialog::AddNewAddress() {
  GtkWidget* address = gtk_expander_new(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_NEW_ADDRESS).c_str());

  // TODO(jhawkins): Implement the address form.

  return address;
}

GtkWidget* AutoFillDialog::AddNewCreditCard() {
  GtkWidget* credit_card = gtk_expander_new(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_NEW_CREDITCARD).c_str());

  // TODO(jhawkins): Implement the credit card form.

  return credit_card;
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowAutoFillDialog(std::vector<AutoFillProfile>* profiles,
                        std::vector<FormGroup>* credit_cards) {
  if (!dialog) {
    dialog = new AutoFillDialog(profiles, credit_cards);
  }
  dialog->Show();
}
