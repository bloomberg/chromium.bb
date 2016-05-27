// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_CHOOSER_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_CHOOSER_BUBBLE_DELEGATE_H_

#include "base/macros.h"
#include "components/bubble/bubble_delegate.h"

class Browser;
class ChooserController;

namespace content {
class RenderFrameHost;
}

// ChooserBubbleDelegate overrides GetName() to identify the bubble
// you define for collecting metrics. Create an instance of this
// class and pass it to BubbleManager::ShowBubble() to show the bubble.
class ChooserBubbleDelegate : public BubbleDelegate {
 public:
  explicit ChooserBubbleDelegate(
      content::RenderFrameHost* owner,
      std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserBubbleDelegate() override;

  // BubbleDelegate:
  std::string GetName() const override;
  std::unique_ptr<BubbleUi> BuildBubbleUi() override;
  const content::RenderFrameHost* OwningFrame() const override;

  ChooserController* chooser_controller() const {
    return chooser_controller_.get();
  }

 private:
  const content::RenderFrameHost* const owning_frame_;
  Browser* browser_;
  std::unique_ptr<ChooserController> chooser_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleDelegate);
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_CHOOSER_BUBBLE_DELEGATE_H_
