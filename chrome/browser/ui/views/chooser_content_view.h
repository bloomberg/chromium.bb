// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHOOSER_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CHOOSER_CONTENT_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/view.h"

class ChooserController;
class ChooserTableModel;

namespace ui {
class TableModel;
}

namespace views {
class StyledLabel;
class StyledLabelListener;
class TableView;
class TableViewObserver;
}

// A bubble or dialog view for choosing among several options in a table.
// Used for WebUSB/WebBluetooth device selection for Chrome and extensions.
class ChooserContentView : public views::View {
 public:
  ChooserContentView(views::TableViewObserver* observer,
                     std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserContentView() override;

  // views::View:
  gfx::Size GetPreferredSize() const override;

  base::string16 GetDialogButtonLabel(ui::DialogButton button) const;
  bool IsDialogButtonEnabled(ui::DialogButton button) const;
  // Ownership of the view is passed to the caller.
  views::StyledLabel* CreateFootnoteView(
      views::StyledLabelListener* listener) const;
  void Accept();
  void Cancel();
  void Close();
  void StyledLabelLinkClicked();
  void UpdateTableModel();

  views::TableView* table_view_for_test() const { return table_view_; }

 private:
  std::unique_ptr<ChooserController> chooser_controller_;
  std::unique_ptr<ChooserTableModel> chooser_table_model_;
  views::TableView* table_view_;

  DISALLOW_COPY_AND_ASSIGN(ChooserContentView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHOOSER_CONTENT_VIEW_H_
