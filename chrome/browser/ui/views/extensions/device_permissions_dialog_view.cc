// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/device_permissions_dialog_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "device/usb/usb_device.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

using device::UsbDevice;
using extensions::DevicePermissionsPrompt;

class DevicePermissionsTableModel
    : public ui::TableModel,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  explicit DevicePermissionsTableModel(
      scoped_refptr<DevicePermissionsPrompt::Prompt> prompt)
      : prompt_(prompt) {
    prompt_->SetObserver(this);
  }

  ~DevicePermissionsTableModel() override { prompt_->SetObserver(nullptr); }

  // ui::TableModel
  int RowCount() override;
  base::string16 GetText(int row, int column) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // extensions::DevicePermissionsPrompt::Prompt::Observer
  void OnDevicesChanged() override;

 private:
  scoped_refptr<DevicePermissionsPrompt::Prompt> prompt_;
  ui::TableModelObserver* observer_ = nullptr;
};

int DevicePermissionsTableModel::RowCount() {
  return prompt_->GetDeviceCount();
}

base::string16 DevicePermissionsTableModel::GetText(int row, int col_id) {
  switch (col_id) {
    case IDS_DEVICE_PERMISSIONS_DIALOG_DEVICE_NAME_COLUMN:
      return prompt_->GetDeviceName(row);
    case IDS_DEVICE_PERMISSIONS_DIALOG_SERIAL_NUMBER_COLUMN:
      return prompt_->GetDeviceSerialNumber(row);
    default:
      NOTREACHED();
      return base::string16();
  }
}

void DevicePermissionsTableModel::SetObserver(
    ui::TableModelObserver* observer) {
  observer_ = observer;
}

void DevicePermissionsTableModel::OnDevicesChanged() {
  if (observer_) {
    observer_->OnModelChanged();
  }
}

DevicePermissionsDialogView::DevicePermissionsDialogView(
    scoped_refptr<DevicePermissionsPrompt::Prompt> prompt)
    : prompt_(prompt) {
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical,
                           views::kButtonHEdgeMarginNew,
                           0,
                           views::kRelatedControlVerticalSpacing);
  SetLayoutManager(layout);

  views::Label* label = new views::Label(prompt_->GetPromptMessage());
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddChildView(label);

  std::vector<ui::TableColumn> table_columns;
  table_columns.push_back(
      ui::TableColumn(IDS_DEVICE_PERMISSIONS_DIALOG_DEVICE_NAME_COLUMN,
                      ui::TableColumn::LEFT,
                      -1,
                      0.8f));
  table_columns.back().title = l10n_util::GetStringUTF16(
      IDS_DEVICE_PERMISSIONS_DIALOG_DEVICE_NAME_COLUMN);
  table_columns.push_back(
      ui::TableColumn(IDS_DEVICE_PERMISSIONS_DIALOG_SERIAL_NUMBER_COLUMN,
                      ui::TableColumn::LEFT,
                      -1,
                      0.2f));
  table_columns.back().title = l10n_util::GetStringUTF16(
      IDS_DEVICE_PERMISSIONS_DIALOG_SERIAL_NUMBER_COLUMN);

  table_model_.reset(new DevicePermissionsTableModel(prompt_));
  table_view_ = new views::TableView(table_model_.get(),
                                     table_columns,
                                     views::TEXT_ONLY,
                                     !prompt_->multiple());

  views::View* table_parent = table_view_->CreateParentIfNecessary();
  AddChildView(table_parent);
  layout->SetFlexForView(table_parent, 1);
}

DevicePermissionsDialogView::~DevicePermissionsDialogView() {
  RemoveAllChildViews(true);
}

void DevicePermissionsDialogView::DeleteDelegate() {
  // Calling prompt_->Dismissed() here ensures it will be called regardless of
  // how the view is closed, including shutdown of the entire view hierarchy.
  prompt_->Dismissed();
  delete this;
}

bool DevicePermissionsDialogView::Accept() {
  for (int index : table_view_->selection_model().selected_indices()) {
    prompt_->GrantDevicePermission(index);
  }
  return true;
}

base::string16 DevicePermissionsDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    return l10n_util::GetStringUTF16(IDS_DEVICE_PERMISSIONS_DIALOG_SELECT);
  }
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

ui::ModalType DevicePermissionsDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 DevicePermissionsDialogView::GetWindowTitle() const {
  return prompt_->GetHeading();
}

gfx::Size DevicePermissionsDialogView::GetPreferredSize() const {
  return gfx::Size(500, 250);
}

void ChromeDevicePermissionsPrompt::ShowDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents());
  if (manager) {
    constrained_window::ShowWebModalDialogViews(
        new DevicePermissionsDialogView(prompt()), web_contents());
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DevicePermissionsPrompt::Prompt::Dismissed, prompt()));
  }
}
