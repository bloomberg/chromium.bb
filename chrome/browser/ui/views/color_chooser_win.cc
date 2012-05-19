// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/views/color_chooser_dialog.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

class ColorChooserWin : public content::ColorChooser,
                        public ColorChooserDialog::Listener,
                        public content::WebContentsObserver {
 public:
  ColorChooserWin(int identifier,
                  content::WebContents* tab,
                  SkColor initial_color);
  ~ColorChooserWin();

  // content::ColorChooser:
  virtual void End() OVERRIDE {}
  virtual void SetSelectedColor(SkColor color) OVERRIDE {}

  // ColorChooserDialog::Listener:
  virtual void DidChooseColor(SkColor color);
  virtual void DidEnd();

 private:
  scoped_refptr<ColorChooserDialog> color_chooser_dialog_;
};

content::ColorChooser* content::ColorChooser::Create(int identifier,
                                                     content::WebContents* tab,
                                                     SkColor initial_color) {
  return new ColorChooserWin(identifier, tab, initial_color);
}

ColorChooserWin::ColorChooserWin(int identifier,
                                 content::WebContents* tab,
                                 SkColor initial_color)
    : content::ColorChooser(identifier),
      content::WebContentsObserver(tab) {
  gfx::NativeWindow owning_window = platform_util::GetTopLevel(
      web_contents()->GetRenderViewHost()->GetView()->GetNativeView());
  color_chooser_dialog_ = new ColorChooserDialog(this,
                                                 initial_color,
                                                 owning_window);
}

ColorChooserWin::~ColorChooserWin() {
  // Always call End() before destroying.
  DCHECK(!color_chooser_dialog_);
}

void ColorChooserWin::DidChooseColor(SkColor color) {
  if (web_contents())
    web_contents()->DidChooseColorInColorChooser(identifier(), color);
}

void ColorChooserWin::DidEnd() {
  if (color_chooser_dialog_.get()) {
    color_chooser_dialog_->ListenerDestroyed();
    color_chooser_dialog_ = NULL;
  }
  if (web_contents())
    web_contents()->DidEndColorChooser(identifier());
}
