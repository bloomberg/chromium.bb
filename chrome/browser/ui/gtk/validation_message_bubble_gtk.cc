// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/validation_message_bubble.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"

namespace {

const int kPadding = 4;
const int kIconTextMargin = 8;
const int kTextMargin = 4;
const int kTextMaxWidth = 256;

// A GTK implementation of ValidationMessageBubble.
class ValidationMessageBubbleGtk : public chrome::ValidationMessageBubble,
                                   public BubbleDelegateGtk {
 public:
  ValidationMessageBubbleGtk(content::RenderWidgetHost* widget_host,
                             const gfx::Rect& anchor_in_screen,
                             const string16& main_text,
                             const string16& sub_text);
  virtual ~ValidationMessageBubbleGtk();

  // BubbleDelegateGtk override:
  virtual void BubbleClosing(BubbleGtk*, bool) OVERRIDE;

 private:
  static GtkWidget* ConstructContent(const string16& main_text,
                                     const string16& sub_text);

  BubbleGtk* bubble_;
};

ValidationMessageBubbleGtk::ValidationMessageBubbleGtk(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_screen,
    const string16& main_text,
    const string16& sub_text)
    : bubble_(NULL) {
  if (!widget_host->IsRenderView())
    return;
  gfx::Rect bounds_in_screen = widget_host->GetView()->GetViewBounds();
  gfx::Rect anchor = anchor_in_screen;
  anchor.Offset(-bounds_in_screen.x(), -bounds_in_screen.y());

  Profile* profile = Profile::FromBrowserContext(
      content::WebContents::FromRenderViewHost(
          content::RenderViewHost::From(widget_host))->GetBrowserContext());
  bubble_ = BubbleGtk::Show(widget_host->GetView()->GetNativeView(),
                            &anchor,
                            ConstructContent(main_text, sub_text),
                            BubbleGtk::ANCHOR_TOP_LEFT,
                            BubbleGtk::POPUP_WINDOW,
                            GtkThemeService::GetFrom(profile),
                            this);
}

ValidationMessageBubbleGtk::~ValidationMessageBubbleGtk() {
  if (bubble_)
    bubble_->Close();
}

void ValidationMessageBubbleGtk::BubbleClosing(BubbleGtk*, bool) {
  bubble_ = NULL;
}

// static
GtkWidget* ValidationMessageBubbleGtk::ConstructContent(
      const string16& main_text, const string16& sub_text) {
  GtkWidget* icon = gtk_image_new();
  gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0);
  gtk_misc_set_padding(GTK_MISC(icon), kPadding, kPadding);
  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(
      *bundle.GetImageSkiaNamed(IDR_INPUT_ALERT)->bitmap());
  gtk_image_set_from_pixbuf(GTK_IMAGE(icon), pixbuf);
  g_object_unref(pixbuf);
  GtkWidget* icon_text_box = gtk_hbox_new(FALSE, kIconTextMargin);
  gtk_container_add(GTK_CONTAINER(icon_text_box), icon);

  GtkWidget* text_box = gtk_vbox_new(FALSE, kTextMargin);
  GtkWidget* label = gtk_label_new(UTF16ToUTF8(main_text).c_str());
  const gfx::Font& main_font = bundle.GetFont(ResourceBundle::MediumFont);
  gtk_util::ForceFontSizePixels(label, main_font.GetHeight());
  gtk_box_pack_start(
      GTK_BOX(text_box), gtk_util::LeftAlignMisc(label), TRUE, TRUE, 0);

  if (!sub_text.empty()) {
    GtkWidget* sub_label = gtk_label_new(UTF16ToUTF8(sub_text).c_str());
    const gfx::Font& sub_font = bundle.GetFont(ResourceBundle::BaseFont);
    gtk_util::ForceFontSizePixels(sub_label, sub_font.GetHeight());
    int max_characters = kTextMaxWidth / sub_font.GetAverageCharacterWidth();
    if (sub_text.length() > static_cast<size_t>(max_characters))
      gtk_util::SetLabelWidth(sub_label, kTextMaxWidth);
    gtk_box_pack_start(
        GTK_BOX(text_box), gtk_util::LeftAlignMisc(sub_label), TRUE, TRUE, 0);
  }
  gtk_container_add(GTK_CONTAINER(icon_text_box), text_box);

  GtkWidget* content = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(
      GTK_ALIGNMENT(content), kPadding, kPadding, kPadding, kPadding);
  gtk_container_add(GTK_CONTAINER(content), icon_text_box);
  return content;
}

}  // namespace

namespace chrome {

scoped_ptr<ValidationMessageBubble> ValidationMessageBubble::CreateAndShow(
    content::RenderWidgetHost* widget_host,
    const gfx::Rect& anchor_in_screen,
    const string16& main_text,
    const string16& sub_text) {
  return scoped_ptr<ValidationMessageBubble>(new ValidationMessageBubbleGtk(
      widget_host, anchor_in_screen, main_text, sub_text)).Pass();
}

}  // namespace chrome
