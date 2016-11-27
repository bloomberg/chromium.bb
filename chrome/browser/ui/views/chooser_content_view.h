// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHOOSER_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CHOOSER_CONTENT_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "ui/base/models/table_model.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

namespace views {
class StyledLabel;
class TableView;
class TableViewObserver;
class Throbber;
}

// A bubble or dialog view for choosing among several options in a table.
// Used for WebUSB/WebBluetooth device selection for Chrome and extensions.
//
// TODO(juncai): change ChooserContentView class name to be more specific.
// https://crbug.com/651568
class ChooserContentView : public views::View,
                           public ui::TableModel,
                           public ChooserController::View,
                           public views::StyledLabelListener {
 public:
  ChooserContentView(views::TableViewObserver* table_view_observer,
                     std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserContentView() override;

  // views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  gfx::ImageSkia GetIcon(int row) override;

  // ChooserController::View:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;
  void OnOptionUpdated(size_t index) override;
  void OnAdapterEnabledChanged(bool enabled) override;
  void OnRefreshStateChanged(bool refreshing) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  base::string16 GetWindowTitle() const;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const;
  bool IsDialogButtonEnabled(ui::DialogButton button) const;
  views::StyledLabel* footnote_link() { return footnote_link_.get(); }
  void Accept();
  void Cancel();
  void Close();
  void UpdateTableView();
  void SetGetHelpAndReScanLink();

 private:
  friend class ChooserContentViewTest;
  friend class ChooserDialogViewTest;
  FRIEND_TEST_ALL_PREFIXES(ChooserContentViewTest, InitialState);
  FRIEND_TEST_ALL_PREFIXES(ChooserContentViewTest, AdapterOnAndOffAndOn);
  FRIEND_TEST_ALL_PREFIXES(ChooserContentViewTest,
                           DiscoveringAndNoOptionAddedAndIdle);
  FRIEND_TEST_ALL_PREFIXES(ChooserContentViewTest,
                           DiscoveringAndOneOptionAddedAndSelectedAndIdle);
  FRIEND_TEST_ALL_PREFIXES(ChooserContentViewTest, ClickRescanLink);
  FRIEND_TEST_ALL_PREFIXES(ChooserContentViewTest, ClickGetHelpLink);

  std::unique_ptr<ChooserController> chooser_controller_;
  views::TableView* table_view_ = nullptr;  // Weak.
  views::View* table_parent_ = nullptr;  // Weak.
  views::StyledLabel* turn_adapter_off_help_ = nullptr;  // Weak.
  views::Throbber* throbber_ = nullptr;  // Weak.
  std::unique_ptr<views::StyledLabel> footnote_link_;
  base::string16 help_text_;
  base::string16 help_and_scanning_text_;
  base::string16 help_and_re_scan_text_;
  gfx::Range help_text_range_;
  gfx::Range re_scan_text_range_;

  DISALLOW_COPY_AND_ASSIGN(ChooserContentView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHOOSER_CONTENT_VIEW_H_
