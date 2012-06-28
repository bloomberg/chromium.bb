// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/chrome_to_mobile_bubble_gtk.h"

#include <gtk/gtk.h>

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_to_mobile_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/image/image.h"

namespace {

// The currently open app-modal bubble singleton, or NULL if none is open.
ChromeToMobileBubbleGtk* g_bubble = NULL;

// Padding between the content and the edge of bubble; from BookmarkBubbleGtk.
const int kContentBorder = 7;

// Horizontal padding that preceeds mobile device radio buttons.
const int kRadioPadding = 20;

// The millisecond duration of the "Sending..." progress throb animation.
const size_t kProgressThrobDurationMS = 2400;

// The seconds to delay before automatically closing the bubble after sending.
const int kAutoCloseDelay = 3;

// The color of the error label.
const GdkColor kErrorColor = GDK_COLOR_RGB(0xFF, 0x00, 0x00);

}  // namespace

// static
void ChromeToMobileBubbleGtk::Show(GtkImage* anchor_image, Browser* browser) {
  // Do not construct a new bubble if one is already being shown.
  if (!g_bubble)
    g_bubble = new ChromeToMobileBubbleGtk(anchor_image, browser);
}

void ChromeToMobileBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                            bool closed_by_escape) {
  DCHECK_EQ(bubble, bubble_);

  // Instruct the service to delete the snapshot file.
  service_->DeleteSnapshot(snapshot_path_);

  // Restore the resting state mobile device icon.
  gtk_image_set_from_pixbuf(GTK_IMAGE(anchor_image_),
      theme_service_->GetImageNamed(IDR_MOBILE)->ToGdkPixbuf());

  labels_.clear();
  anchor_image_ = NULL;
  send_copy_ = NULL;
  cancel_ = NULL;
  send_ = NULL;
  error_ = NULL;
  bubble_ = NULL;
  progress_animation_.reset();
}

void ChromeToMobileBubbleGtk::AnimationProgressed(
    const ui::Animation* animation) {
  DCHECK(animation == progress_animation_.get());
  double animation_value = animation->GetCurrentValue();
  int id = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_3;
  // Show each of four messages for 1/4 of the animation.
  if (animation_value < 0.25)
    id = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_0;
  else if (animation_value < 0.5)
    id = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_1;
  else if (animation_value < 0.75)
    id = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_2;
  gtk_button_set_label(GTK_BUTTON(send_), l10n_util::GetStringUTF8(id).c_str());
}

void ChromeToMobileBubbleGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED);

  // Update the tracked labels to match the new theme, as in BookmarkBubbleGtk.
  const GdkColor* color =
      theme_service_->UsingNativeTheme() ? NULL : &ui::kGdkBlack;
  std::vector<GtkWidget*>::iterator it;
  for (it = labels_.begin(); it != labels_.end(); ++it)
    gtk_widget_modify_fg(*it, GTK_STATE_NORMAL, color);
}

void ChromeToMobileBubbleGtk::SnapshotGenerated(const FilePath& path,
                                                int64 bytes) {
  snapshot_path_ = path;
  if (bytes > 0) {
    service_->LogMetric(ChromeToMobileService::SNAPSHOT_GENERATED);
    gtk_button_set_label(GTK_BUTTON(send_copy_), l10n_util::GetStringFUTF8(
        IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY, ui::FormatBytes(bytes)).c_str());
    gtk_widget_set_sensitive(send_copy_, TRUE);
  } else {
    service_->LogMetric(ChromeToMobileService::SNAPSHOT_ERROR);
    gtk_button_set_label(GTK_BUTTON(send_copy_), l10n_util::GetStringUTF8(
        IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY_FAILED).c_str());
  }
}

void ChromeToMobileBubbleGtk::OnSendComplete(bool success) {
  progress_animation_->Stop();
  gtk_button_set_alignment(GTK_BUTTON(send_), 0.5, 0.5);

  if (success) {
    gtk_button_set_label(GTK_BUTTON(send_),
        l10n_util::GetStringUTF8(IDS_CHROME_TO_MOBILE_BUBBLE_SENT).c_str());
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&ChromeToMobileBubbleGtk::OnCancelClicked,
                   weak_ptr_factory_.GetWeakPtr(), GTK_WIDGET(NULL)),
        base::TimeDelta::FromSeconds(kAutoCloseDelay));
  } else {
    gtk_button_set_label(GTK_BUTTON(send_),
        l10n_util::GetStringUTF8(IDS_CHROME_TO_MOBILE_BUBBLE_ERROR).c_str());
    gtk_widget_set_visible(error_, TRUE);
  }
}

ChromeToMobileBubbleGtk::ChromeToMobileBubbleGtk(GtkImage* anchor_image,
                                                 Browser* browser)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      browser_(browser),
      service_(ChromeToMobileServiceFactory::GetForProfile(browser->profile())),
      theme_service_(GtkThemeService::GetFrom(browser->profile())),
      selected_mobile_(NULL),
      anchor_image_(anchor_image),
      send_copy_(NULL),
      cancel_(NULL),
      send_(NULL),
      error_(NULL),
      bubble_(NULL) {
  service_->LogMetric(ChromeToMobileService::BUBBLE_SHOWN);

  // Generate the MHTML snapshot now to report its size in the bubble.
  service_->GenerateSnapshot(browser_, weak_ptr_factory_.GetWeakPtr());

  // Request a mobile device list update.
  service_->RequestMobileListUpdate();

  // Get the list of mobile devices.
  std::vector<DictionaryValue*> mobiles = service_->mobiles();
  DCHECK_GT(mobiles.size(), 0U);
  selected_mobile_ = mobiles[0];

  GtkWidget* content = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(content), kContentBorder);

  // Create and pack the title label; init the selected mobile device.
  GtkWidget* title = NULL;
  if (mobiles.size() == 1) {
    string16 mobile_name;
    mobiles[0]->GetString("name", &mobile_name);
    title = gtk_label_new(l10n_util::GetStringFUTF8(
        IDS_CHROME_TO_MOBILE_BUBBLE_SINGLE_TITLE, mobile_name).c_str());
  } else {
    title = gtk_label_new(l10n_util::GetStringUTF8(
        IDS_CHROME_TO_MOBILE_BUBBLE_MULTI_TITLE).c_str());
  }
  gtk_misc_set_alignment(GTK_MISC(title), 0, 1);
  gtk_box_pack_start(GTK_BOX(content), title, FALSE, FALSE, 0);
  labels_.push_back(title);

  // Create and pack the device radio group; init the selected mobile device.
  if (mobiles.size() > 1) {
    GtkWidget* radio = NULL;
    for (std::vector<DictionaryValue*>::const_iterator it = mobiles.begin();
         it != mobiles.end(); ++it) {
      std::string name;
      (*it)->GetStringASCII("name", &name);
      radio = gtk_radio_button_new_with_label_from_widget(
                  GTK_RADIO_BUTTON(radio), name.c_str());

      // Activate the default radio button before attaching its signal handler.
      if (mobile_map_.empty()) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
        DCHECK_EQ(selected_mobile_, *it);
      }

      mobile_map_[radio] = *it;
      // Pack each radio item into a horizontal box padding.
      GtkWidget* row = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(row), radio, FALSE, FALSE, kRadioPadding);
      gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
      g_signal_connect(radio, "toggled", G_CALLBACK(OnRadioToggledThunk), this);
    }
  }

  // Create and pack the offline copy check box.
  send_copy_ = gtk_check_button_new_with_label(
      l10n_util::GetStringFUTF8(IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY,
          l10n_util::GetStringUTF16(
              IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY_GENERATING)).c_str());
  gtk_widget_set_sensitive(send_copy_, FALSE);
  gtk_box_pack_start(GTK_BOX(content), send_copy_, FALSE, FALSE, 0);

  // Set the send button requested size from its final (presumed longest) string
  // to avoid resizes during animation. Use the same size for the cancel button.
  send_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_3).c_str());
  GtkRequisition button_size;
  gtk_widget_size_request(send_, &button_size);
  button_size.width *= 1.25;
  gtk_widget_set_size_request(send_, button_size.width, button_size.height);
  gtk_button_set_label(GTK_BUTTON(send_),
      l10n_util::GetStringUTF8(IDS_CHROME_TO_MOBILE_BUBBLE_SEND).c_str());
  cancel_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CANCEL).c_str());
  gtk_widget_set_size_request(cancel_, button_size.width, button_size.height);

  // Pack the buttons with an expanding label for right-justification.
  GtkWidget* buttons = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(buttons), gtk_label_new(""), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(buttons), cancel_, FALSE, FALSE, 4);
  gtk_box_pack_start(GTK_BOX(buttons), send_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content), buttons, FALSE, FALSE, 0);

  // Pack the error label, but ensure it doesn't show initially with the bubble.
  error_ = gtk_label_new(l10n_util::GetStringUTF8(
      IDS_CHROME_TO_MOBILE_BUBBLE_ERROR_MESSAGE).c_str());
  gtk_misc_set_alignment(GTK_MISC(error_), 0, 1);
  gtk_util::SetLabelColor(error_, &kErrorColor);
  gtk_box_pack_start(GTK_BOX(content), error_, FALSE, FALSE, 0);
  gtk_widget_set_no_show_all(error_, TRUE);

  // Initialize focus to the send button.
  gtk_container_set_focus_child(GTK_CONTAINER(content), send_);

  BubbleGtk::ArrowLocationGtk arrow_location = base::i18n::IsRTL() ?
      BubbleGtk::ARROW_LOCATION_TOP_LEFT : BubbleGtk::ARROW_LOCATION_TOP_RIGHT;
  bubble_ = BubbleGtk::Show(GTK_WIDGET(anchor_image_),
                            NULL,
                            content,
                            arrow_location,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service_,
                            this /*delegate*/);
  if (!bubble_) {
    NOTREACHED();
    return;
  }

  g_signal_connect(content, "destroy", G_CALLBACK(&OnDestroyThunk), this);
  g_signal_connect(cancel_, "clicked", G_CALLBACK(&OnCancelClickedThunk), this);
  g_signal_connect(send_, "clicked", G_CALLBACK(&OnSendClickedThunk), this);

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  theme_service_->InitThemesFor(this);

  gtk_image_set_from_pixbuf(GTK_IMAGE(anchor_image_),
      theme_service_->GetImageNamed(IDR_MOBILE_LIT)->ToGdkPixbuf());
}

ChromeToMobileBubbleGtk::~ChromeToMobileBubbleGtk() {
  DCHECK(g_bubble);
  g_bubble = NULL;
}

void ChromeToMobileBubbleGtk::OnDestroy(GtkWidget* widget) {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroyed (via the BubbleGtk being destroyed), and delete ourself.
  delete this;
}

void ChromeToMobileBubbleGtk::OnRadioToggled(GtkWidget* widget) {
  DCHECK(mobile_map_.find(widget) != mobile_map_.end());
  selected_mobile_ = mobile_map_.find(widget)->second;
}

void ChromeToMobileBubbleGtk::OnCancelClicked(GtkWidget* widget) {
  bubble_->Close();
}

void ChromeToMobileBubbleGtk::OnSendClicked(GtkWidget* widget) {
  string16 mobile_id;
  selected_mobile_->GetString("id", &mobile_id);
  bool send_copy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(send_copy_));
  service_->SendToMobile(mobile_id, send_copy ? snapshot_path_ : FilePath(),
                         browser_, weak_ptr_factory_.GetWeakPtr());

  // Update the view's contents to show the "Sending..." progress animation.
  gtk_widget_set_sensitive(cancel_, FALSE);
  gtk_widget_set_sensitive(send_, FALSE);
  gtk_button_set_alignment(GTK_BUTTON(send_), 0, 0.5);
  progress_animation_.reset(new ui::ThrobAnimation(this));
  progress_animation_->SetDuration(kProgressThrobDurationMS);
  progress_animation_->StartThrobbing(-1);
}
