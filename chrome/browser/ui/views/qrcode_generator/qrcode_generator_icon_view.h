// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class CommandUpdater;

namespace qrcode_generator {

// The location bar icon to show the QR Code generator bubble, where the user
// can generate a QR code for the current page or a selected image.
class QRCodeGeneratorIconView : public PageActionIconView {
 public:
  QRCodeGeneratorIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  ~QRCodeGeneratorIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  void UpdateImpl() override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;
  bool ShouldShowLabel() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  const char* GetClassName() const override;

 private:
  // Flag set when OnExecuting() is called and cleared after bubble is created.
  // Avoids losing ink drop on, or flickering, the PageActionIconView.
  bool bubble_requested_;

  DISALLOW_COPY_AND_ASSIGN(QRCodeGeneratorIconView);
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_ICON_VIEW_H_
