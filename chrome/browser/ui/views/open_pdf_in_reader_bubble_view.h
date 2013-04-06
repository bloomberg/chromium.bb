// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class OpenPDFInReaderPromptDelegate;

namespace views {
class LabelButton;
}

class OpenPDFInReaderBubbleView : public views::BubbleDelegateView,
                                  public views::ButtonListener,
                                  public views::LinkListener {
 public:
  OpenPDFInReaderBubbleView(views::View* anchor_view,
                            OpenPDFInReaderPromptDelegate* model);
  virtual ~OpenPDFInReaderBubbleView();

 protected:
  // views::BubbleDelegateView:
  virtual void Init() OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  // Weak pointer; owned by the PDFTabHelper of the currently active tab.
  OpenPDFInReaderPromptDelegate* model_;

  views::Link* open_in_reader_link_;
  views::LabelButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(OpenPDFInReaderBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_
