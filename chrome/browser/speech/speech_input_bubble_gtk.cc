// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_bubble.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/info_bubble_gtk.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "media/audio/audio_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/rect.h"

namespace {

const int kBubbleControlVerticalSpacing = 5;
const int kBubbleControlHorizontalSpacing = 20;
const int kIconHorizontalPadding = 10;
const int kButtonBarHorizontalSpacing = 10;

// Use black for text labels since the bubble has white background.
const GdkColor kLabelTextColor = gtk_util::kGdkBlack;

// Implementation of SpeechInputBubble for GTK. This shows a speech input
// info bubble on screen.
class SpeechInputBubbleGtk
    : public SpeechInputBubbleBase,
      public InfoBubbleGtkDelegate {
 public:
  SpeechInputBubbleGtk(TabContents* tab_contents,
                       Delegate* delegate,
                       const gfx::Rect& element_rect);
  ~SpeechInputBubbleGtk();

 private:
  // InfoBubbleDelegate methods.
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape);

  // SpeechInputBubble methods.
  virtual void Show();
  virtual void Hide();
  virtual void UpdateLayout();
  virtual void UpdateImage();

  CHROMEGTK_CALLBACK_0(SpeechInputBubbleGtk, void, OnCancelClicked);
  CHROMEGTK_CALLBACK_0(SpeechInputBubbleGtk, void, OnTryAgainClicked);
  CHROMEGTK_CALLBACK_0(SpeechInputBubbleGtk, void, OnMicSettingsClicked);

  Delegate* delegate_;
  InfoBubbleGtk* info_bubble_;
  gfx::Rect element_rect_;
  bool did_invoke_close_;

  GtkWidget* label_;
  GtkWidget* cancel_button_;
  GtkWidget* try_again_button_;
  GtkWidget* icon_;
  GtkWidget* icon_container_;
  GtkWidget* mic_settings_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputBubbleGtk);
};

SpeechInputBubbleGtk::SpeechInputBubbleGtk(TabContents* tab_contents,
                                           Delegate* delegate,
                                           const gfx::Rect& element_rect)
    : SpeechInputBubbleBase(tab_contents),
      delegate_(delegate),
      info_bubble_(NULL),
      element_rect_(element_rect),
      did_invoke_close_(false),
      label_(NULL),
      cancel_button_(NULL),
      try_again_button_(NULL),
      icon_(NULL),
      icon_container_(NULL),
      mic_settings_(NULL) {
}

SpeechInputBubbleGtk::~SpeechInputBubbleGtk() {
  // The |Close| call below invokes our |InfoBubbleClosing| method. Since we
  // were destroyed by the caller we don't need to call them back, hence set
  // this flag here.
  did_invoke_close_ = true;
  Hide();
}

void SpeechInputBubbleGtk::InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                             bool closed_by_escape) {
  info_bubble_ = NULL;
  if (!did_invoke_close_)
    delegate_->InfoBubbleFocusChanged();
}

void SpeechInputBubbleGtk::OnCancelClicked(GtkWidget* widget) {
  delegate_->InfoBubbleButtonClicked(BUTTON_CANCEL);
}

void SpeechInputBubbleGtk::OnTryAgainClicked(GtkWidget* widget) {
  delegate_->InfoBubbleButtonClicked(BUTTON_TRY_AGAIN);
}

void SpeechInputBubbleGtk::OnMicSettingsClicked(GtkWidget* widget) {
  AudioManager::GetAudioManager()->ShowAudioInputSettings();
}

void SpeechInputBubbleGtk::Show() {
  if (info_bubble_)
    return;  // Nothing further to do since the bubble is already visible.

  // We use a vbox to arrange the controls (label, image, button bar) vertically
  // and the button bar is a hbox holding the 2 buttons (try again and cancel).
  // To get horizontal space around them we place this vbox with padding in a
  // GtkAlignment below.
  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

  // The icon with a some padding on the left and right.
  icon_container_ = gtk_alignment_new(0, 0, 0, 0);
  icon_ = gtk_image_new();
  gtk_container_add(GTK_CONTAINER(icon_container_), icon_);
  gtk_box_pack_start(GTK_BOX(vbox), icon_container_, FALSE, FALSE,
                     kBubbleControlVerticalSpacing);

  label_ = gtk_label_new(NULL);
  gtk_util::SetLabelColor(label_, &kLabelTextColor);
  gtk_box_pack_start(GTK_BOX(vbox), label_, FALSE, FALSE,
                     kBubbleControlVerticalSpacing);

  if (AudioManager::GetAudioManager()->CanShowAudioInputSettings()) {
    mic_settings_ = gtk_chrome_link_button_new(
        l10n_util::GetStringUTF8(IDS_SPEECH_INPUT_MIC_SETTINGS).c_str());
    gtk_box_pack_start(GTK_BOX(vbox), mic_settings_, FALSE, FALSE,
                       kBubbleControlVerticalSpacing);
    g_signal_connect(mic_settings_, "clicked",
                     G_CALLBACK(&OnMicSettingsClickedThunk), this);
  }

  GtkWidget* button_bar = gtk_hbox_new(FALSE, kButtonBarHorizontalSpacing);
  gtk_box_pack_start(GTK_BOX(vbox), button_bar, FALSE, FALSE,
                     kBubbleControlVerticalSpacing);

  cancel_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CANCEL).c_str());
  gtk_box_pack_start(GTK_BOX(button_bar), cancel_button_, TRUE, FALSE, 0);
  g_signal_connect(cancel_button_, "clicked",
                   G_CALLBACK(&OnCancelClickedThunk), this);

  try_again_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_SPEECH_INPUT_TRY_AGAIN).c_str());
  gtk_box_pack_start(GTK_BOX(button_bar), try_again_button_, TRUE, FALSE, 0);
  g_signal_connect(try_again_button_, "clicked",
                   G_CALLBACK(&OnTryAgainClickedThunk), this);

  GtkWidget* content = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(content),
      kBubbleControlVerticalSpacing, kBubbleControlVerticalSpacing,
      kBubbleControlHorizontalSpacing, kBubbleControlHorizontalSpacing);
  gtk_container_add(GTK_CONTAINER(content), vbox);

  GtkThemeService* theme_provider = GtkThemeService::GetFrom(
      tab_contents()->profile());
  gfx::Rect rect(
      element_rect_.x() + element_rect_.width() - kBubbleTargetOffsetX,
      element_rect_.y() + element_rect_.height(), 1, 1);
  info_bubble_ = InfoBubbleGtk::Show(tab_contents()->GetNativeView(),
                                     &rect,
                                     content,
                                     InfoBubbleGtk::ARROW_LOCATION_TOP_LEFT,
                                     false,  // match_system_theme
                                     true,  // grab_input
                                     theme_provider,
                                     this);

  UpdateLayout();
}

void SpeechInputBubbleGtk::Hide() {
  if (info_bubble_)
    info_bubble_->Close();
}

void SpeechInputBubbleGtk::UpdateLayout() {
  if (!info_bubble_)
    return;

  if (display_mode() == DISPLAY_MODE_MESSAGE) {
    // Message text and the Try Again + Cancel buttons are visible, hide the
    // icon.
    gtk_label_set_text(GTK_LABEL(label_),
                       UTF16ToUTF8(message_text()).c_str());
    gtk_widget_show(label_);
    gtk_widget_show(try_again_button_);
    if (mic_settings_)
      gtk_widget_show(mic_settings_);
    gtk_widget_hide(icon_);
  } else {
    // Heading text, icon and cancel button are visible, hide the Try Again
    // button.
    gtk_label_set_text(GTK_LABEL(label_),
        l10n_util::GetStringUTF8(IDS_SPEECH_INPUT_BUBBLE_HEADING).c_str());
    if (display_mode() == DISPLAY_MODE_RECORDING) {
      gtk_widget_show(label_);
    } else {
      gtk_widget_hide(label_);
    }
    UpdateImage();
    gtk_widget_show(icon_);
    gtk_widget_hide(try_again_button_);
    if (mic_settings_)
      gtk_widget_hide(mic_settings_);
    if (display_mode() == DISPLAY_MODE_WARM_UP) {
      gtk_widget_hide(cancel_button_);

      // The text label and cancel button are hidden in this mode, but we want
      // the popup to appear the same size as it would once recording starts,
      // so as to reduce UI jank when recording starts. So we calculate the
      // difference in size between the two sets of controls and add that as
      // padding around the icon here.
      GtkRequisition cancel_size;
      gtk_widget_get_child_requisition(cancel_button_, &cancel_size);
      GtkRequisition label_size;
      gtk_widget_get_child_requisition(label_, &label_size);
      SkBitmap* volume = ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_SPEECH_INPUT_MIC_EMPTY);
      int desired_width = std::max(volume->width(), cancel_size.width) +
                          kIconHorizontalPadding * 2;
      int desired_height = volume->height() + label_size.height +
                           cancel_size.height +
                           kBubbleControlVerticalSpacing * 2;
      int diff_width = desired_width - icon_image().width();
      int diff_height = desired_height - icon_image().height();
      gtk_alignment_set_padding(GTK_ALIGNMENT(icon_container_),
                                diff_height / 2, diff_height - diff_height / 2,
                                diff_width / 2, diff_width - diff_width / 2);
    } else {
      // Reset the padding done above.
      gtk_alignment_set_padding(GTK_ALIGNMENT(icon_container_), 0, 0,
                                kIconHorizontalPadding, kIconHorizontalPadding);
      gtk_widget_show(cancel_button_);
    }
  }
}

void SpeechInputBubbleGtk::UpdateImage() {
  SkBitmap image = icon_image();
  if (image.isNull() || !info_bubble_)
    return;

  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&image);
  gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), pixbuf);
  g_object_unref(pixbuf);
}

}  // namespace

SpeechInputBubble* SpeechInputBubble::CreateNativeBubble(
    TabContents* tab_contents,
    Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechInputBubbleGtk(tab_contents, delegate, element_rect);
}

