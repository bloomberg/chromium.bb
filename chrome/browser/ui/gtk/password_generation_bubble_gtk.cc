// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/password_generation_bubble_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/password_generator.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::RenderViewHost;

namespace {

GdkPixbuf* GetImage(int resource_id) {
  if (!resource_id)
    return NULL;
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
    resource_id, ui::ResourceBundle::RTL_ENABLED).ToGdkPixbuf();
}

}  // namespace

PasswordGenerationBubbleGtk::PasswordGenerationBubbleGtk(
    const gfx::Rect& anchor_rect,
    const content::PasswordForm& form,
    content::WebContents* web_contents,
    autofill::PasswordGenerator* password_generator)
    : form_(form),
      web_contents_(web_contents),
      password_generator_(password_generator) {
  // TODO(gcasto): Localize text after we have finalized the UI.
  // crbug.com/118062
  GtkWidget* content = gtk_vbox_new(FALSE, 5);

  // We have two lines of content. The first is the title and learn more link.
  GtkWidget* title_line = gtk_hbox_new(FALSE,  0);
  GtkWidget* title = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_PASSWORD_GENERATION_BUBBLE_TITLE).c_str());
  gtk_box_pack_start(GTK_BOX(title_line), title, FALSE, FALSE, 0);
  GtkWidget* learn_more_link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_LEARN_MORE).c_str());
  gtk_button_set_alignment(GTK_BUTTON(learn_more_link), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(title_line),
                     gtk_util::IndentWidget(learn_more_link),
                     FALSE, FALSE, 0);

  // The second contains the password in a text field, a regenerate button, and
  // an accept button.
  GtkWidget* password_line = gtk_hbox_new(FALSE, ui::kControlSpacing);
  text_field_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(text_field_),
                     password_generator_->Generate().c_str());
  gtk_entry_set_max_length(GTK_ENTRY(text_field_), 15);
  gtk_entry_set_icon_from_pixbuf(
      GTK_ENTRY(text_field_), GTK_ENTRY_ICON_SECONDARY, GetImage(IDR_RELOAD));
  gtk_entry_set_icon_tooltip_text(
      GTK_ENTRY(text_field_), GTK_ENTRY_ICON_SECONDARY, "Regenerate");
  GtkWidget* accept_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_PASSWORD_GENERATION_BUTTON_TEXT).c_str());
  gtk_box_pack_start(GTK_BOX(password_line), text_field_, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(password_line), accept_button, TRUE, TRUE, 0);

  gtk_container_set_border_width(GTK_CONTAINER(content),
                                 ui::kContentAreaBorder);
  gtk_box_pack_start(GTK_BOX(content), title_line, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), password_line, TRUE, TRUE, 0);

  // Set initial focus to the text field containing the generated password.
  gtk_widget_grab_focus(text_field_);

  GtkThemeService* theme_service = GtkThemeService::GetFrom(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  bubble_ = BubbleGtk::Show(web_contents->GetView()->GetContentNativeView(),
                            &anchor_rect,
                            content,
                            BubbleGtk::ANCHOR_TOP_LEFT,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service,
                            this);  // delegate

  g_signal_connect(content, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);
  g_signal_connect(accept_button, "clicked",
                   G_CALLBACK(&OnAcceptClickedThunk), this);
  g_signal_connect(text_field_, "icon-press",
                   G_CALLBACK(&OnRegenerateClickedThunk), this);
  g_signal_connect(text_field_, "changed",
                   G_CALLBACK(&OnPasswordEditedThunk), this);
  g_signal_connect(learn_more_link, "clicked",
                   G_CALLBACK(OnLearnMoreLinkClickedThunk), this);
}

PasswordGenerationBubbleGtk::~PasswordGenerationBubbleGtk() {}

void PasswordGenerationBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                                bool closed_by_escape) {
  password_generation::LogUserActions(actions_);
}

void PasswordGenerationBubbleGtk::OnDestroy(GtkWidget* widget) {
  // We are self deleting, we have a destroy signal setup to catch when we are
  // destroyed (via the BubbleGtk being destroyed), and delete ourself.
  delete this;
}

void PasswordGenerationBubbleGtk::OnAcceptClicked(GtkWidget* widget) {
  actions_.password_accepted = true;
  RenderViewHost* render_view_host = web_contents_->GetRenderViewHost();
  render_view_host->Send(new AutofillMsg_GeneratedPasswordAccepted(
      render_view_host->GetRoutingID(),
      UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(text_field_)))));
  PasswordManager::FromWebContents(web_contents_)->
      SetFormHasGeneratedPassword(form_);
  bubble_->Close();
}

void PasswordGenerationBubbleGtk::OnRegenerateClicked(
    GtkWidget* widget,
    GtkEntryIconPosition icon_pos,
    GdkEvent* event) {
  gtk_entry_set_text(GTK_ENTRY(text_field_),
                     password_generator_->Generate().c_str());
  actions_.password_regenerated = true;
}

void PasswordGenerationBubbleGtk::OnPasswordEdited(GtkWidget* widget) {
  actions_.password_edited = true;
}

void PasswordGenerationBubbleGtk::OnLearnMoreLinkClicked(GtkButton* button) {
  actions_.learn_more_visited = true;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  content::OpenURLParams params(
      GURL(chrome::kAutoPasswordGenerationLearnMoreURL), content::Referrer(),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false);
  browser->OpenURL(params);
  bubble_->Close();
}
