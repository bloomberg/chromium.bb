// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/color_chooser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/views/color_chooser/color_chooser_listener.h"
#include "ui/views/color_chooser/color_chooser_view.h"
#include "ui/views/widget/widget.h"


namespace {

class ColorChooserAura : public content::ColorChooser,
                         public views::ColorChooserListener {
 public:
  ColorChooserAura(int identifier,
                   content::WebContents* tab,
                   SkColor initial_color);

 private:
  // content::ColorChooser overrides:
  virtual void End() OVERRIDE;
  virtual void SetSelectedColor(SkColor color) OVERRIDE;

  // views::ColorChooserListener overrides:
  virtual void OnColorChosen(SkColor color) OVERRIDE;
  virtual void OnColorChooserDialogClosed() OVERRIDE;

  // The web contents invoking the color chooser.  No ownership because it will
  // outlive this class.
  content::WebContents* tab_;

  // The actual view of the color chooser.  No ownership because its parent
  // view will take care of its lifetime.
  views::ColorChooserView* view_;

  // The widget for the color chooser.  No ownership because it's released
  // automatically when closed.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(ColorChooserAura);
};

ColorChooserAura::ColorChooserAura(int identifier,
                                   content::WebContents* tab,
                                   SkColor initial_color)
    : ColorChooser(identifier),
      tab_(tab) {
  DCHECK(tab_);
  view_ = new views::ColorChooserView(this, initial_color);
  widget_ = views::Widget::CreateWindowWithContext(
      view_, tab->GetView()->GetNativeView());
  widget_->SetAlwaysOnTop(true);
  widget_->Show();
}

void ColorChooserAura::OnColorChosen(SkColor color) {
  tab_->DidChooseColorInColorChooser(identifier(), color);
}

void ColorChooserAura::OnColorChooserDialogClosed() {
  view_ = NULL;
  widget_ = NULL;
  tab_->DidEndColorChooser(identifier());
}

void ColorChooserAura::End() {
  if (widget_ && widget_->IsVisible()) {
    view_->set_listener(NULL);
    widget_->Close();
    view_ = NULL;
    widget_ = NULL;
    // DidEndColorChooser will invoke Browser::DidEndColorChooser, which deletes
    // this. Take care of the call order.
    tab_->DidEndColorChooser(identifier());
  }
}

void ColorChooserAura::SetSelectedColor(SkColor color) {
  if (view_)
    view_->OnColorChanged(color);
}

}  // namespace

// static
content::ColorChooser* content::ColorChooser::Create(
    int identifier, content::WebContents* tab, SkColor initial_color) {
  return new ColorChooserAura(identifier, tab, initial_color);
}
