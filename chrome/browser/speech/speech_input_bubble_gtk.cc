// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_bubble.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/browser/gtk/owned_widget_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "gfx/gtk_util.h"
#include "gfx/rect.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

const int kBubbleControlVerticalSpacing = 10;
const int kBubbleControlHorizontalSpacing = 50;

// Use black for text labels since the bubble has white background.
const GdkColor kLabelTextColor = gfx::kGdkBlack;

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

  CHROMEGTK_CALLBACK_0(SpeechInputBubbleGtk, void, OnCancelClicked);

  Delegate* delegate_;
  InfoBubbleGtk* info_bubble_;
  GtkWidget* icon_;
  OwnedWidgetGtk content_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputBubbleGtk);
};

SpeechInputBubbleGtk::SpeechInputBubbleGtk(TabContents* tab_contents,
                                           Delegate* delegate,
                                           const gfx::Rect& element_rect)
    : delegate_(delegate) {
  // We use a vbox to arrange the 3 controls (label, image, button) vertically.
  // To get horizontal space around them we place this vbox with padding in a
  // GtkAlignment below.
  GtkWidget* vbox = gtk_vbox_new(FALSE, kBubbleControlVerticalSpacing);

  GtkWidget* heading_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_SPEECH_INPUT_BUBBLE_HEADING).c_str());
  gtk_widget_modify_fg(heading_label, GTK_STATE_NORMAL, &kLabelTextColor);
  gtk_box_pack_start(GTK_BOX(vbox), heading_label, FALSE, FALSE, 0);

  SkBitmap* image = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_SPEECH_INPUT_RECORDING);
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(image);
  icon_ = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);
  gtk_box_pack_start(GTK_BOX(vbox), icon_, FALSE, FALSE, 0);

  GtkWidget* cancel_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CANCEL).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), cancel_button, FALSE, FALSE, 0);
  g_signal_connect(cancel_button, "clicked",
                   G_CALLBACK(&OnCancelClickedThunk), this);

  content_.Own(gtk_alignment_new(0, 0, 0, 0));
  gtk_alignment_set_padding(GTK_ALIGNMENT(content_.get()),
      kBubbleControlVerticalSpacing, kBubbleControlVerticalSpacing,
      kBubbleControlHorizontalSpacing, kBubbleControlHorizontalSpacing);
  gtk_container_add(GTK_CONTAINER(content_.get()), vbox);

  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(
      tab_contents->profile());
  gfx::Rect rect(element_rect.x() + kBubbleTargetOffsetX,
                 element_rect.y() + element_rect.height(), 1, 1);
  info_bubble_ = InfoBubbleGtk::Show(tab_contents->GetNativeView(),
                                     &rect,
                                     content_.get(),
                                     InfoBubbleGtk::ARROW_LOCATION_TOP_LEFT,
                                     false,  // match_system_theme
                                     true,  // grab_input
                                     theme_provider,
                                     this);
}

SpeechInputBubbleGtk::~SpeechInputBubbleGtk() {
  // The |Close| call below invokes our |InfoBubbleClosing| method. Since we
  // were destroyed by the caller we don't need to call them back, hence set
  // the delegate to NULL here.
  delegate_ = NULL;
  content_.Destroy();
  info_bubble_->Close();
}

void SpeechInputBubbleGtk::InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                             bool closed_by_escape) {
  if (delegate_)
    delegate_->InfoBubbleFocusChanged();
}

void SpeechInputBubbleGtk::OnCancelClicked(GtkWidget* widget) {
  delegate_->InfoBubbleButtonClicked(BUTTON_CANCEL);
}

void SpeechInputBubbleGtk::Show() {
  // TODO(satish): Implement.
  NOTREACHED();
}

void SpeechInputBubbleGtk::Hide() {
  // TODO(satish): Implement.
  NOTREACHED();
}

void SpeechInputBubbleGtk::UpdateLayout() {
  // TODO: Implement.
  NOTREACHED();
}

}  // namespace

SpeechInputBubble* SpeechInputBubble::CreateNativeBubble(
    TabContents* tab_contents,
    Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechInputBubbleGtk(tab_contents, delegate, element_rect);
}

