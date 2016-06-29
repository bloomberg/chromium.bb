// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/origin.h"

class ChooserContentView;
class ChooserController;

namespace content {
class WebContents;
}

namespace views {
class TableView;
}

// Displays a chooser view as a modal dialog constrained
// to the window/tab displaying the given web contents.
class ChooserDialogView : public views::DialogDelegateView,
                          public views::StyledLabelListener,
                          public views::TableViewObserver {
 public:
  ChooserDialogView(content::WebContents* web_contents,
                    std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserDialogView() override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  ui::ModalType GetModalType() const override;

  // views::DialogDelegate:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* CreateFootnoteView() override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  // views::DialogDelegateView:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;

  views::TableView* table_view_for_test() const;

 private:
  content::WebContents* web_contents_;
  url::Origin origin_;
  ChooserContentView* chooser_content_view_;

  DISALLOW_COPY_AND_ASSIGN(ChooserDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_CHOOSER_DIALOG_VIEW_H_
