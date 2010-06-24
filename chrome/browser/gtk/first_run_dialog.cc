// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/first_run_dialog.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

// static
bool FirstRunDialog::Show(Profile* profile,
                          ProcessSingleton* process_singleton) {
  int response = -1;
  // Object deletes itself.
  FirstRunDialog* first_run = new FirstRunDialog(profile, response);

  // Prevent further launches of Chrome until First Run UI is done.
  process_singleton->Lock(GTK_WINDOW(first_run->dialog_));

  // TODO(port): it should be sufficient to just run the dialog:
  // int response = gtk_dialog_run(GTK_DIALOG(dialog));
  // but that spins a nested message loop and hoses us.  :(
  // http://code.google.com/p/chromium/issues/detail?id=12552
  // Instead, run a loop and extract the response manually.
  g_signal_connect(first_run->dialog_, "response",
                   G_CALLBACK(OnResponseDialogThunk), first_run);
  gtk_widget_show_all(first_run->dialog_);
  MessageLoop::current()->Run();

  process_singleton->Unlock();
  return (response == GTK_RESPONSE_ACCEPT);
}

FirstRunDialog::FirstRunDialog(Profile* profile, int& response)
    : dialog_(NULL), report_crashes_(NULL), make_default_(NULL),
      import_data_(NULL), import_profile_(NULL), profile_(profile),
      response_(response), importer_host_(new ImporterHost()) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_FIRSTRUN_DLG_TITLE).c_str(),
      NULL,  // No parent
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_QUIT,
      GTK_RESPONSE_REJECT,
      NULL);
  gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(IDS_FIRSTRUN_DLG_OK).c_str(),
      GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT);

  // Normally we would do the following:
  //   gtk_widget_realize(dialog_);
  //   gtk_util::SetWindowSizeFromResources(GTK_WINDOW(dialog_),
  //                                        IDS_FIRSTRUN_DIALOG_WIDTH_CHARS,
  //                                        -1,
  //                                        false);  // resizable
  // But because the first run dialog has extra widgets in Windows, the
  // resources specify a dialog that is way too big.  So instead in just this
  // one case we let GTK size the dialog itself and just mark it non-resizable
  // manually:
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);

  g_signal_connect(dialog_, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);

  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), 18);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 12);

#if defined(GOOGLE_CHROME_BUILD)
  GtkWidget* check_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_OPTIONS_ENABLE_LOGGING).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(check_label), TRUE);

  GtkWidget* learn_more_link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_LEARN_MORE).c_str());
  // Stick it in an hbox so it doesn't expand to the whole width.
  GtkWidget* learn_more_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(learn_more_hbox),
                     gtk_util::IndentWidget(learn_more_link),
                     FALSE, FALSE, 0);
  g_signal_connect(learn_more_link, "clicked",
                   G_CALLBACK(OnLearnMoreLinkClickedThunk), this);

  report_crashes_ = gtk_check_button_new();
  gtk_container_add(GTK_CONTAINER(report_crashes_), check_label);

  GtkWidget* report_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(report_vbox), report_crashes_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(report_vbox), learn_more_hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), report_vbox, FALSE, FALSE, 0);
#endif

  make_default_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FR_CUSTOMIZE_DEFAULT_BROWSER).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), make_default_, FALSE, FALSE, 0);

  GtkWidget* combo_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  import_data_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FR_CUSTOMIZE_IMPORT).c_str());
  gtk_box_pack_start(GTK_BOX(combo_hbox), import_data_, FALSE, FALSE, 0);
  import_profile_ = gtk_combo_box_new_text();
  gtk_box_pack_start(GTK_BOX(combo_hbox), import_profile_, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), combo_hbox, FALSE, FALSE, 0);

  // Detect any supported browsers that we can import from and fill
  // up the combo box. If none found, disable import data checkbox.
  int profiles_count = importer_host_->GetAvailableProfileCount();
  if (profiles_count > 0) {
    for (int i = 0; i < profiles_count; i++) {
      std::wstring profile = importer_host_->GetSourceProfileNameAt(i);
      gtk_combo_box_append_text(GTK_COMBO_BOX(import_profile_),
                                WideToUTF8(profile).c_str());
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(import_data_), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(import_profile_), 0);
  } else {
    gtk_combo_box_append_text(GTK_COMBO_BOX(import_profile_),
        l10n_util::GetStringUTF8(IDS_IMPORT_NO_PROFILE_FOUND).c_str());
    gtk_combo_box_set_active(GTK_COMBO_BOX(import_profile_), 0);
    gtk_widget_set_sensitive(import_data_, FALSE);
    gtk_widget_set_sensitive(import_profile_, FALSE);
  }

  gtk_box_pack_start(GTK_BOX(content_area), vbox, FALSE, FALSE, 0);
}

void FirstRunDialog::OnResponseDialog(GtkWidget* widget, int response) {
  bool import_started = false;
  gtk_widget_hide_all(dialog_);
  response_ = response;

  if (response == GTK_RESPONSE_ACCEPT) {
    // Mark that first run has ran.
    FirstRun::CreateSentinel();

    // Check if user has opted into reporting.
    if (report_crashes_ &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(report_crashes_))) {
#if defined(USE_LINUX_BREAKPAD)
      if (GoogleUpdateSettings::SetCollectStatsConsent(true)) {
        InitCrashReporter();
      }
#endif
    } else {
      GoogleUpdateSettings::SetCollectStatsConsent(false);
    }

    // If selected set as default browser.
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(make_default_)))
      ShellIntegration::SetAsDefaultBrowser();

    // Import data if selected.
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(import_data_))) {
      const ProfileInfo& source_profile =
          importer_host_->GetSourceProfileInfoAt(
          gtk_combo_box_get_active(GTK_COMBO_BOX(import_profile_)));
      int items = importer::SEARCH_ENGINES + importer::HISTORY +
          importer::FAVORITES + importer::HOME_PAGE + importer::PASSWORDS;
      // TODO(port): Should we do the actual import in a new process like
      // Windows?
      StartImportingWithUI(GTK_WINDOW(dialog_), items, importer_host_.get(),
                           source_profile, profile_, this, true);
      import_started = true;
    }
  }
  if (!import_started)
    FirstRunDone();
}

void FirstRunDialog::OnLearnMoreLinkClicked(GtkButton* button) {
  platform_util::OpenExternal(GURL(
      l10n_util::GetStringUTF8(IDS_LEARN_MORE_REPORTING_URL)));
}

void FirstRunDialog::FirstRunDone() {
  // Set preference to show first run bubble and welcome page.
  FirstRun::SetShowFirstRunBubblePref(true);
  FirstRun::SetShowWelcomePagePref();

  gtk_widget_destroy(dialog_);
  MessageLoop::current()->Quit();
  delete this;
}
