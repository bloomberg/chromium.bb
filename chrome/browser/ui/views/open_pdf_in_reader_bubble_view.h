// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/link_listener.h"

namespace pdf {
class OpenPDFInReaderPromptClient;
}

class OpenPDFInReaderBubbleView : public views::BubbleDialogDelegateView,
                                  public views::LinkListener {
 public:
  OpenPDFInReaderBubbleView(views::View* anchor_view,
                            pdf::OpenPDFInReaderPromptClient* model);
  ~OpenPDFInReaderBubbleView() override;

 protected:
  // views::BubbleDialogDelegateView:
  void Init() override;
  bool Cancel() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

 private:
  // Weak pointer; owned by the PDFWebContentsHelper of the currently active
  // tab.
  pdf::OpenPDFInReaderPromptClient* model_;

  views::Link* open_in_reader_link_;

  DISALLOW_COPY_AND_ASSIGN(OpenPDFInReaderBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPEN_PDF_IN_READER_BUBBLE_VIEW_H_
