// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_ANDROID_ANDROID_UI_H_
#define MANDOLINE_UI_BROWSER_ANDROID_ANDROID_UI_H_

#include "base/macros.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/ui/browser/browser_ui.h"

namespace mojo {
class Shell;
class View;
}

namespace mandoline {

class Browser;

class AndroidUI : public BrowserUI,
                  public mojo::ViewObserver {
 public:
  AndroidUI(Browser* browser, mojo::ApplicationImpl* application_impl);
  ~AndroidUI() override;

 private:
  // Overridden from BrowserUI:
  void Init(mojo::View* root) override;
  void EmbedOmnibox(mojo::ApplicationConnection* connection) override;
  void OnURLChanged() override;
  void LoadingStateChanged(bool loading) override;
  void ProgressChanged(double progress) override;

  // Overriden from mojo::ViewObserver:
  virtual void OnViewBoundsChanged(mojo::View* view,
                                   const mojo::Rect& old_bounds,
                                   const mojo::Rect& new_bounds) override;

  Browser* browser_;
  mojo::ApplicationImpl* application_impl_;
  mojo::View* root_;

  DISALLOW_COPY_AND_ASSIGN(AndroidUI);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_ANDROID_ANDROID_UI_H_
