// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

namespace pdf {
class OpenPDFInReaderPromptClient;
}

namespace views {
class LabelButton;
}

class OpenPDFInReaderBubbleView : public views::BubbleDelegateView,
                                  public views::ButtonListener,
                                  public views::LinkListener {
 public:
  OpenPDFInReaderBubbleView(views::View* anchor_view,
                            pdf::OpenPDFInReaderPromptClient* model);
  ~OpenPDFInReaderBubbleView() override;

 protected:
  // views::BubbleDelegateView:
  void Init() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

 private:
  // Weak pointer; owned by the PDFWebContentsHelper of the currently active
  // tab.
  pdf::OpenPDFInReaderPromptClient* model_;

  views::Link* open_in_reader_link_;
  views::LabelButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(OpenPDFInReaderBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_
