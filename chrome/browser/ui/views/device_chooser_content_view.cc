// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_chooser_content_view.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/widget/widget.h"

namespace {

const int kThrobberDiameter = 24;

const int kAdapterOffHelpLinkPadding = 5;

// The lookup table for signal strength level image.
const int kSignalStrengthLevelImageIds[5] = {IDR_SIGNAL_0_BAR, IDR_SIGNAL_1_BAR,
                                             IDR_SIGNAL_2_BAR, IDR_SIGNAL_3_BAR,
                                             IDR_SIGNAL_4_BAR};

}  // namespace

DeviceChooserContentView::DeviceChooserContentView(
    views::TableViewObserver* table_view_observer,
    std::unique_ptr<ChooserController> chooser_controller)
    : chooser_controller_(std::move(chooser_controller)),
      help_text_(l10n_util::GetStringFUTF16(
          IDS_DEVICE_CHOOSER_GET_HELP_LINK_WITH_SCANNING_STATUS,
          base::string16())),
      help_and_scanning_text_(l10n_util::GetStringFUTF16(
          IDS_DEVICE_CHOOSER_GET_HELP_LINK_WITH_SCANNING_STATUS,
          l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING))) {
  base::string16 re_scan_text =
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN);
  std::vector<size_t> offsets;
  help_and_re_scan_text_ = l10n_util::GetStringFUTF16(
      IDS_DEVICE_CHOOSER_GET_HELP_LINK_WITH_RE_SCAN_LINK, help_text_,
      re_scan_text, &offsets);
  help_text_range_ = gfx::Range(offsets[0], offsets[0] + help_text_.size());
  re_scan_text_range_ =
      gfx::Range(offsets[1], offsets[1] + re_scan_text.size());
  chooser_controller_->set_view(this);
  std::vector<ui::TableColumn> table_columns;
  table_columns.push_back(ui::TableColumn());
  table_view_ = new views::TableView(
      this, table_columns,
      chooser_controller_->ShouldShowIconBeforeText() ? views::ICON_AND_TEXT
                                                      : views::TEXT_ONLY,
      !chooser_controller_->AllowMultipleSelection() /* single_selection */);
  table_view_->set_select_on_remove(false);
  table_view_->set_observer(table_view_observer);
  table_view_->SetEnabled(chooser_controller_->NumOptions() > 0);

  table_parent_ = table_view_->CreateParentIfNecessary();
  AddChildView(table_parent_);

  throbber_ = new views::Throbber();
  throbber_->SetVisible(false);
  AddChildView(throbber_);

  base::string16 link_text = l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_TURN_ON_BLUETOOTH_LINK_TEXT);
  size_t offset = 0;
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_TURN_ADAPTER_OFF, link_text, &offset);
  turn_adapter_off_help_ = new views::StyledLabel(text, this);
  turn_adapter_off_help_->AddStyleRange(
      gfx::Range(0, link_text.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());
  turn_adapter_off_help_->SetVisible(false);
  AddChildView(turn_adapter_off_help_);
}

DeviceChooserContentView::~DeviceChooserContentView() {
  chooser_controller_->set_view(nullptr);
  table_view_->set_observer(nullptr);
  table_view_->SetModel(nullptr);
}

void DeviceChooserContentView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  table_parent_->SetBoundsRect(rect);
  // Set the throbber in the center of the chooser.
  throbber_->SetBounds((rect.width() - kThrobberDiameter) / 2,
                       (rect.height() - kThrobberDiameter) / 2,
                       kThrobberDiameter, kThrobberDiameter);
  // Set the adapter off message in the center of the chooser.
  // The adapter off message will only be shown when the adapter is off,
  // and in that case, the system won't be able to scan for devices, so
  // the throbber won't be shown at the same time.
  turn_adapter_off_help_->SetPosition(
      gfx::Point((rect.width() - turn_adapter_off_help_->width()) / 2,
                 (rect.height() - turn_adapter_off_help_->height()) / 2));
  turn_adapter_off_help_->SizeToFit(rect.width() -
                                    2 * kAdapterOffHelpLinkPadding);
  views::View::Layout();
}

gfx::Size DeviceChooserContentView::GetPreferredSize() const {
  constexpr int kHeight = 320;
  constexpr int kDefaultWidth = 402;
  int width = LayoutDelegate::Get()->GetDialogPreferredWidth(
      LayoutDelegate::DialogWidth::MEDIUM);
  if (!width)
    width = kDefaultWidth;
  return gfx::Size(width, kHeight);
}

int DeviceChooserContentView::RowCount() {
  // When there are no devices, the table contains a message saying there
  // are no devices, so the number of rows is always at least 1.
  return std::max(base::checked_cast<int>(chooser_controller_->NumOptions()),
                  1);
}

base::string16 DeviceChooserContentView::GetText(int row, int column_id) {
  int num_options = base::checked_cast<int>(chooser_controller_->NumOptions());
  if (num_options == 0) {
    DCHECK_EQ(0, row);
    return chooser_controller_->GetNoOptionsText();
  }

  DCHECK_GE(row, 0);
  DCHECK_LT(row, num_options);
  base::string16 text =
      chooser_controller_->GetOption(static_cast<size_t>(row));
  return chooser_controller_->IsPaired(row)
             ? l10n_util::GetStringFUTF16(
                   IDS_DEVICE_CHOOSER_DEVICE_NAME_AND_PAIRED_STATUS_TEXT, text)
             : text;
}

void DeviceChooserContentView::SetObserver(ui::TableModelObserver* observer) {}

gfx::ImageSkia DeviceChooserContentView::GetIcon(int row) {
  DCHECK(chooser_controller_->ShouldShowIconBeforeText());

  size_t num_options = chooser_controller_->NumOptions();
  if (num_options == 0) {
    DCHECK_EQ(0, row);
    return gfx::ImageSkia();
  }

  DCHECK_GE(row, 0);
  DCHECK_LT(row, base::checked_cast<int>(num_options));

  if (chooser_controller_->IsConnected(row))
    return gfx::CreateVectorIcon(kBluetoothConnectedIcon, gfx::kChromeIconGrey);

  int level = chooser_controller_->GetSignalStrengthLevel(row);

  if (level == -1)
    return gfx::ImageSkia();

  DCHECK_GE(level, 0);
  DCHECK_LT(level, static_cast<int>(arraysize(kSignalStrengthLevelImageIds)));

  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kSignalStrengthLevelImageIds[level]);
}

void DeviceChooserContentView::OnOptionsInitialized() {
  table_view_->OnModelChanged();
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionAdded(size_t index) {
  table_view_->OnItemsAdded(base::checked_cast<int>(index), 1);
  UpdateTableView();
  table_view_->SetVisible(true);
  throbber_->SetVisible(false);
  throbber_->Stop();
}

void DeviceChooserContentView::OnOptionRemoved(size_t index) {
  table_view_->OnItemsRemoved(base::checked_cast<int>(index), 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionUpdated(size_t index) {
  table_view_->OnItemsChanged(base::checked_cast<int>(index), 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnAdapterEnabledChanged(bool enabled) {
  // No row is selected since the adapter status has changed.
  // This will also disable the OK button if it was enabled because
  // of a previously selected row.
  table_view_->Select(-1);
  UpdateTableView();
  table_view_->SetVisible(enabled);
  turn_adapter_off_help_->SetVisible(!enabled);

  throbber_->Stop();
  throbber_->SetVisible(false);

  if (enabled) {
    SetGetHelpAndReScanLink();
  } else {
    DCHECK(footnote_link_);
    footnote_link_->SetText(help_text_);
    footnote_link_->AddStyleRange(
        help_text_range_, views::StyledLabel::RangeStyleInfo::CreateForLink());
  }

  if (GetWidget() && GetWidget()->GetRootView())
    GetWidget()->GetRootView()->Layout();
}

void DeviceChooserContentView::OnRefreshStateChanged(bool refreshing) {
  if (refreshing) {
    // No row is selected since the chooser is refreshing. This will also
    // disable the OK button if it was enabled because of a previously
    // selected row.
    table_view_->Select(-1);
    UpdateTableView();
  }

  // When refreshing and no option available yet, hide |table_view_| and show
  // |throbber_|. Otherwise show |table_view_| and hide |throbber_|.
  bool throbber_visible =
      refreshing && (chooser_controller_->NumOptions() == 0);
  table_view_->SetVisible(!throbber_visible);
  throbber_->SetVisible(throbber_visible);
  if (throbber_visible)
    throbber_->Start();
  else
    throbber_->Stop();

  if (refreshing) {
    DCHECK(footnote_link_);
    footnote_link_->SetText(help_and_scanning_text_);
    footnote_link_->AddStyleRange(
        help_text_range_, views::StyledLabel::RangeStyleInfo::CreateForLink());
  } else {
    SetGetHelpAndReScanLink();
  }

  if (GetWidget() && GetWidget()->GetRootView())
    GetWidget()->GetRootView()->Layout();
}

void DeviceChooserContentView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  if (label == turn_adapter_off_help_) {
    chooser_controller_->OpenAdapterOffHelpUrl();
  } else if (label == footnote_link_.get()) {
    if (range == help_text_range_)
      chooser_controller_->OpenHelpCenterUrl();
    else if (range == re_scan_text_range_)
      chooser_controller_->RefreshOptions();
    else
      NOTREACHED();
  } else {
    NOTREACHED();
  }
}

base::string16 DeviceChooserContentView::GetWindowTitle() const {
  return chooser_controller_->GetTitle();
}

base::string16 DeviceChooserContentView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK
             ? chooser_controller_->GetOkButtonLabel()
             : l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_CANCEL_BUTTON_TEXT);
}

bool DeviceChooserContentView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK ||
         !table_view_->selection_model().empty();
}

views::StyledLabel* DeviceChooserContentView::footnote_link() {
  if (chooser_controller_->ShouldShowFootnoteView()) {
    footnote_link_ = base::MakeUnique<views::StyledLabel>(help_text_, this);
    footnote_link_->set_owned_by_client();
    footnote_link_->AddStyleRange(
        help_text_range_, views::StyledLabel::RangeStyleInfo::CreateForLink());
  }

  return footnote_link_.get();
}

void DeviceChooserContentView::Accept() {
  std::vector<size_t> indices(
      table_view_->selection_model().selected_indices().begin(),
      table_view_->selection_model().selected_indices().end());
  chooser_controller_->Select(indices);
}

void DeviceChooserContentView::Cancel() {
  chooser_controller_->Cancel();
}

void DeviceChooserContentView::Close() {
  chooser_controller_->Close();
}

void DeviceChooserContentView::UpdateTableView() {
  if (chooser_controller_->NumOptions() == 0) {
    table_view_->OnModelChanged();
    table_view_->SetEnabled(false);
  } else {
    table_view_->SetEnabled(true);
  }
}

void DeviceChooserContentView::SetGetHelpAndReScanLink() {
  DCHECK(footnote_link_);
  footnote_link_->SetText(help_and_re_scan_text_);
  footnote_link_->AddStyleRange(
      help_text_range_, views::StyledLabel::RangeStyleInfo::CreateForLink());
  footnote_link_->AddStyleRange(
      re_scan_text_range_, views::StyledLabel::RangeStyleInfo::CreateForLink());
}
