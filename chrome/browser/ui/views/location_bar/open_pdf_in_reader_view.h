// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OPEN_PDF_IN_READER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OPEN_PDF_IN_READER_VIEW_H_

#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget_observer.h"

class OpenPDFInReaderBubbleView;

namespace content {
class WebContents;
}

namespace pdf {
class OpenPDFInReaderPromptClient;
}

// A Page Action image view for the "Open PDF in Reader" bubble.
class OpenPDFInReaderView : public views::ImageView,
                            public views::WidgetObserver {
 public:
  OpenPDFInReaderView();
  ~OpenPDFInReaderView() override;

  void Update(content::WebContents* web_contents);

 private:
  void ShowBubble();

  // views::ImageView:
  void GetAccessibleState(ui::AXViewState* state) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  OpenPDFInReaderBubbleView* bubble_;

  // Weak pointer; owned by the PDFWebContentsHelper of the currently active
  // tab.
  pdf::OpenPDFInReaderPromptClient* model_;

  DISALLOW_COPY_AND_ASSIGN(OpenPDFInReaderView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OPEN_PDF_IN_READER_VIEW_H_
