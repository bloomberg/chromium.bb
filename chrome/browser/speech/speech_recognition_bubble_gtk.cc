// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_bubble.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/rect.h"

using content::WebContents;

namespace {

const int kBubbleControlVerticalSpacing = 5;
const int kBubbleControlHorizontalSpacing = 20;
const int kIconHorizontalPadding = 10;
const int kButtonBarHorizontalSpacing = 10;

// Use black for text labels since the bubble has white background.
const GdkColor& kLabelTextColor = ui::kGdkBlack;

// Implementation of SpeechRecognitionBubble for GTK. This shows a speech
// recognition bubble on screen.
class SpeechRecognitionBubbleGtk : public SpeechRecognitionBubbleBase,
                                   public BubbleDelegateGtk {
 public:
  SpeechRecognitionBubbleGtk(WebContents* web_contents,
                             Delegate* delegate,
                             const gfx::Rect& element_rect);
  ~SpeechRecognitionBubbleGtk();

 private:
  // SpeechRecognitionBubbleBase:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void UpdateLayout() OVERRIDE;
  virtual void UpdateImage() OVERRIDE;

  // BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  CHROMEGTK_CALLBACK_0(SpeechRecognitionBubbleGtk, void, OnCancelClicked);
  CHROMEGTK_CALLBACK_0(SpeechRecognitionBubbleGtk, void, OnTryAgainClicked);
  CHROMEGTK_CALLBACK_0(SpeechRecognitionBubbleGtk, void, OnMicSettingsClicked);

  Delegate* delegate_;
  BubbleGtk* bubble_;
  gfx::Rect element_rect_;
  bool did_invoke_close_;

  GtkWidget* label_;
  GtkWidget* cancel_button_;
  GtkWidget* try_again_button_;
  GtkWidget* icon_;
  GtkWidget* icon_container_;
  GtkWidget* mic_settings_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionBubbleGtk);
};

SpeechRecognitionBubbleGtk::SpeechRecognitionBubbleGtk(
    WebContents* web_contents, Delegate* delegate,
    const gfx::Rect& element_rect)
    : SpeechRecognitionBubbleBase(web_contents),
      delegate_(delegate),
      bubble_(NULL),
      element_rect_(element_rect),
      did_invoke_close_(false),
      label_(NULL),
      cancel_button_(NULL),
      try_again_button_(NULL),
      icon_(NULL),
      icon_container_(NULL),
      mic_settings_(NULL) {
}

SpeechRecognitionBubbleGtk::~SpeechRecognitionBubbleGtk() {
  // The |Close| call below invokes our |BubbleClosing| method. Since we were
  // destroyed by the caller we don't need to call them back, hence set this
  // flag here.
  did_invoke_close_ = true;
  Hide();
}

void SpeechRecognitionBubbleGtk::OnCancelClicked(GtkWidget* widget) {
  delegate_->InfoBubbleButtonClicked(BUTTON_CANCEL);
}

void SpeechRecognitionBubbleGtk::OnTryAgainClicked(GtkWidget* widget) {
  delegate_->InfoBubbleButtonClicked(BUTTON_TRY_AGAIN);
}

void SpeechRecognitionBubbleGtk::OnMicSettingsClicked(GtkWidget* widget) {
  content::SpeechRecognitionManager::GetInstance()->ShowAudioInputSettings();
  Hide();
}

void SpeechRecognitionBubbleGtk::Show() {
  if (bubble_)
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

  Profile* profile = Profile::FromBrowserContext(
      GetWebContents()->GetBrowserContext());

  // TODO(tommi): The audio_manager property can only be accessed from the
  // IO thread, so we can't call CanShowAudioInputSettings directly here if
  // we can show the input settings.  For now, we always show the link (like
  // we do on other platforms).
  if (true) {
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

  GtkThemeService* theme_provider = GtkThemeService::GetFrom(profile);
  GtkWidget* reference_widget = GetWebContents()->GetNativeView();
  gfx::Rect container_rect;
  GetWebContents()->GetContainerBounds(&container_rect);
  gfx::Rect target_rect(element_rect_.right() - kBubbleTargetOffsetX,
      element_rect_.bottom(), 1, 1);

  if (target_rect.x() < 0 || target_rect.y() < 0 ||
      target_rect.x() > container_rect.width() ||
      target_rect.y() > container_rect.height()) {
    // Target is not in screen view, so point to wrench.
    Browser* browser =
        browser::FindOrCreateTabbedBrowser(profile);
    BrowserWindowGtk* browser_window =
        BrowserWindowGtk::GetBrowserWindowForNativeWindow(
            browser->window()->GetNativeHandle());
    reference_widget = browser_window->GetToolbar()->GetLocationBarView()
        ->location_icon_widget();
    target_rect = gtk_util::WidgetBounds(reference_widget);
  }
  bubble_ = BubbleGtk::Show(reference_widget,
                            &target_rect,
                            content,
                            BubbleGtk::ARROW_LOCATION_TOP_LEFT,
                            false,  // match_system_theme
                            true,  // grab_input
                            theme_provider,
                            this);

  UpdateLayout();
}

void SpeechRecognitionBubbleGtk::Hide() {
  if (bubble_)
    bubble_->Close();
}

void SpeechRecognitionBubbleGtk::UpdateLayout() {
  if (!bubble_)
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

void SpeechRecognitionBubbleGtk::UpdateImage() {
  SkBitmap image = icon_image();
  if (image.isNull() || !bubble_)
    return;

  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&image);
  gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), pixbuf);
  g_object_unref(pixbuf);
}

void SpeechRecognitionBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                               bool closed_by_escape) {
  bubble_ = NULL;
  if (!did_invoke_close_)
    delegate_->InfoBubbleFocusChanged();
}

}  // namespace

SpeechRecognitionBubble* SpeechRecognitionBubble::CreateNativeBubble(
    WebContents* web_contents,
    Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechRecognitionBubbleGtk(web_contents, delegate, element_rect);
}
