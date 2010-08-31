// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/first_run_dialog.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_floating_container.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "gfx/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

namespace {

const gchar* kSearchEngineKey = "template-url-search-engine";

// Height of the label that displays the search engine's logo (in lieu of the
// actual logo) in chromium.
const int kLogoLabelHeight = 100;

// The width of the explanatory label. The 180 is the width of the large images.
const int kExplanationWidth = 3 * 180;

// Horizontal spacing between search engine choices.
const int kSearchEngineSpacing = 6;

// Set the (x, y) coordinates of the welcome message (which floats on top of
// the omnibox image at the top of the first run dialog).
void SetWelcomePosition(GtkFloatingContainer* container,
                        GtkAllocation* allocation,
                        GtkWidget* label) {
  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);
  g_value_set_int(&value, gtk_util::kContentAreaSpacing);
  gtk_container_child_set_property(GTK_CONTAINER(container),
                                   label, "x", &value);

  GtkRequisition req;
  gtk_widget_size_request(label, &req);
  int y = allocation->height / 2 - req.height / 2;

  g_value_set_int(&value, y);
  gtk_container_child_set_property(GTK_CONTAINER(container),
                                   label, "y", &value);
  g_value_unset(&value);
}

}  // namespace

// static
bool FirstRunDialog::Show(Profile* profile,
                          bool randomize_search_engine_order) {
  int response = -1;
  // Object deletes itself.
  new FirstRunDialog(profile, randomize_search_engine_order, response);

  // TODO(port): it should be sufficient to just run the dialog:
  // int response = gtk_dialog_run(GTK_DIALOG(dialog));
  // but that spins a nested message loop and hoses us.  :(
  // http://code.google.com/p/chromium/issues/detail?id=12552
  // Instead, run a loop and extract the response manually.
  MessageLoop::current()->Run();

  return (response == GTK_RESPONSE_ACCEPT);
}

FirstRunDialog::FirstRunDialog(Profile* profile,
                               bool randomize_search_engine_order,
                               int& response)
    : search_engine_window_(NULL),
      dialog_(NULL),
      report_crashes_(NULL),
      make_default_(NULL),
      profile_(profile),
      chosen_search_engine_(NULL),
      response_(response) {
  search_engines_model_ = profile_->GetTemplateURLModel();
  ShowSearchEngineWindow();

  search_engines_model_->AddObserver(this);
  if (search_engines_model_->loaded())
    OnTemplateURLModelChanged();
  else
    search_engines_model_->Load();
}

FirstRunDialog::~FirstRunDialog() {
}

void FirstRunDialog::ShowSearchEngineWindow() {
  search_engine_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(
      GTK_WINDOW(search_engine_window_),
      l10n_util::GetStringUTF8(IDS_FIRSTRUN_DLG_TITLE).c_str());
  gtk_window_set_resizable(GTK_WINDOW(search_engine_window_), FALSE);
  g_signal_connect(search_engine_window_, "destroy",
                   G_CALLBACK(OnSearchEngineWindowDestroyThunk), this);
  GtkWidget* content_area = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(search_engine_window_), content_area);

  GdkPixbuf* pixbuf =
      ResourceBundle::GetSharedInstance().GetPixbufNamed(
          IDR_SEARCH_ENGINE_DIALOG_TOP);
  GtkWidget* top_image = gtk_image_new_from_pixbuf(pixbuf);
  // Right align the image.
  gtk_misc_set_alignment(GTK_MISC(top_image), 1, 0);
  gtk_widget_set_size_request(top_image, 0, -1);

  GtkWidget* welcome_message = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(IDS_FR_SEARCH_MAIN_LABEL));

  GtkWidget* top_area = gtk_floating_container_new();
  gtk_container_add(GTK_CONTAINER(top_area), top_image);
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(top_area),
                                      welcome_message);
  g_signal_connect(top_area, "set-floating-position",
                   G_CALLBACK(SetWelcomePosition), welcome_message);

  gtk_box_pack_start(GTK_BOX(content_area), top_area,
                     FALSE, FALSE, 0);

  GtkWidget* bubble_area_background = gtk_event_box_new();
  gtk_widget_modify_bg(bubble_area_background,
                       GTK_STATE_NORMAL, &gfx::kGdkWhite);

  GtkWidget* bubble_area_box = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_area_box),
                                 gtk_util::kContentAreaSpacing);
  gtk_container_add(GTK_CONTAINER(bubble_area_background),
                    bubble_area_box);

  GtkWidget* explanation = gtk_label_new(
      l10n_util::GetStringFUTF8(IDS_FR_SEARCH_TEXT,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)).c_str());
  gtk_util::SetLabelColor(explanation, &gfx::kGdkBlack);
  gtk_label_set_line_wrap(GTK_LABEL(explanation), TRUE);
  gtk_widget_set_size_request(explanation, kExplanationWidth, -1);
  gtk_box_pack_start(GTK_BOX(bubble_area_box), explanation, FALSE, FALSE, 0);

  // We will fill this in after the TemplateURLModel has loaded.
  search_engine_hbox_ = gtk_hbox_new(FALSE, kSearchEngineSpacing);
  gtk_box_pack_start(GTK_BOX(bubble_area_box), search_engine_hbox_,
                     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(content_area), bubble_area_background,
                     TRUE, TRUE, 0);

  gtk_widget_show_all(content_area);
  gtk_window_present(GTK_WINDOW(search_engine_window_));
}

void FirstRunDialog::ShowDialog() {
  // The purpose of the dialog is to ask the user to enable stats and crash
  // reporting. This setting may be controlled through configuration management
  // in enterprise scenarios. If that is the case, skip the dialog entirely,
  // it's not worth bothering the user for only the default browser question
  // (which is likely to be forced in enterprise deployments anyway).
  const PrefService::Preference* metrics_reporting_pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kMetricsReportingEnabled);
  if (metrics_reporting_pref && metrics_reporting_pref->IsManaged()) {
    OnResponseDialog(NULL, GTK_RESPONSE_ACCEPT);
    return;
  }

#if defined(GOOGLE_CHROME_BUILD)
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

  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);

  g_signal_connect(dialog_, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);

  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;

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

  gtk_box_pack_start(GTK_BOX(content_area), report_crashes_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content_area), learn_more_hbox, FALSE, FALSE, 0);

  make_default_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FR_CUSTOMIZE_DEFAULT_BROWSER).c_str());
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(make_default_), TRUE);
  gtk_box_pack_start(GTK_BOX(content_area), make_default_, FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnResponseDialogThunk), this);
  gtk_widget_show_all(dialog_);
#else  // !defined(GOOGLE_CHROME_BUILD)
  // We don't show the dialog in chromium. Pretend the user accepted.
  OnResponseDialog(NULL, GTK_RESPONSE_ACCEPT);
#endif  // !defined(GOOGLE_CHROME_BUILD)
}

void FirstRunDialog::OnTemplateURLModelChanged() {
  // We only watch the search engine model change once, on load.  Remove
  // observer so we don't try to redraw if engines change under us.
  search_engines_model_->RemoveObserver(this);

  // Add search engines in search_engines_model_ to buttons list.  The
  // first three will always be from prepopulated data.
  std::vector<const TemplateURL*> template_urls =
      search_engines_model_->GetTemplateURLs();

  std::vector<const TemplateURL*>::iterator search_engine_iter;

  std::string choose_text = l10n_util::GetStringUTF8(IDS_FR_SEARCH_CHOOSE);
  for (std::vector<const TemplateURL*>::iterator search_engine_iter =
           template_urls.begin();
       search_engine_iter < template_urls.end() &&
           search_engine_iter < template_urls.begin() + 3;
       ++search_engine_iter) {
    // Create a container for the search engine widgets.
    GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

    // We show text on Chromium and images on Google Chrome.
    bool show_images = false;
#if defined(GOOGLE_CHROME_BUILD)
    show_images = true;
#endif

    // Create the image (maybe).
    int logo_id = (*search_engine_iter)->logo_id();
    if (show_images && logo_id > 0) {
      GdkPixbuf* pixbuf =
          ResourceBundle::GetSharedInstance().GetPixbufNamed(logo_id);
      GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
      gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);
    } else {
      GtkWidget* logo_label = gtk_label_new(NULL);
      char* markup = g_markup_printf_escaped(
          "<span weight='bold' size='x-large' color='black'>%s</span>",
          WideToUTF8((*search_engine_iter)->short_name()).c_str());
      gtk_label_set_markup(GTK_LABEL(logo_label), markup);
      g_free(markup);
      gtk_widget_set_size_request(logo_label, -1, kLogoLabelHeight);
      gtk_box_pack_start(GTK_BOX(vbox), logo_label, FALSE, FALSE, 0);
    }

    // Create the button.
    GtkWidget* button = gtk_button_new_with_label(choose_text.c_str());
    g_signal_connect(button, "clicked",
                     G_CALLBACK(OnSearchEngineButtonClickedThunk), this);
    g_object_set_data(G_OBJECT(button), kSearchEngineKey,
                      const_cast<TemplateURL*>(*search_engine_iter));

    GtkWidget* button_centerer = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_centerer), button, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button_centerer, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(search_engine_hbox_), vbox, TRUE, TRUE, 0);
    gtk_widget_show_all(search_engine_hbox_);
  }
}

void FirstRunDialog::OnSearchEngineButtonClicked(GtkWidget* sender) {
  chosen_search_engine_ = static_cast<TemplateURL*>(
      g_object_get_data(G_OBJECT(sender), kSearchEngineKey));
  gtk_widget_destroy(search_engine_window_);
}

void FirstRunDialog::OnSearchEngineWindowDestroy(GtkWidget* sender) {
  search_engine_window_ = NULL;
  if (chosen_search_engine_) {
    search_engines_model_->SetDefaultSearchProvider(chosen_search_engine_);
    ShowDialog();
  } else {
    FirstRunDone();
  }
}

void FirstRunDialog::OnResponseDialog(GtkWidget* widget, int response) {
  if (dialog_)
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
    if (make_default_ &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(make_default_))) {
      ShellIntegration::SetAsDefaultBrowser();
    }
  }

  FirstRunDone();
}

void FirstRunDialog::OnLearnMoreLinkClicked(GtkButton* button) {
  platform_util::OpenExternal(GURL(
      l10n_util::GetStringUTF8(IDS_LEARN_MORE_REPORTING_URL)));
}

void FirstRunDialog::FirstRunDone() {
  FirstRun::SetShowWelcomePagePref();

  if (dialog_)
    gtk_widget_destroy(dialog_);
  MessageLoop::current()->Quit();
  delete this;
}
