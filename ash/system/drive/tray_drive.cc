// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/drive/tray_drive.h"

#include <vector>

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/stl_util.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace internal {

namespace {

const int kSidePadding = 8;
const int kHorizontalPadding = 6;
const int kVerticalPadding = 6;
const int kTopPadding = 6;
const int kBottomPadding = 10;
const int kProgressBarWidth = 100;
const int kProgressBarHeight = 8;

string16 GetTrayLabel(const ash::DriveOperationStatusList& list) {
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DRIVE_SYNCING,
      base::IntToString16(static_cast<int>(list.size())));
}

ash::DriveOperationStatusList* GetCurrentOperationList() {
  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->tray_delegate();
  ash::DriveOperationStatusList* list = new ash::DriveOperationStatusList();
  delegate->GetDriveOperationStatusList(list);
  return list;
}

}

namespace tray {


class DriveDefaultView : public TrayItemMore {
 public:
  DriveDefaultView(SystemTrayItem* owner,
                   const DriveOperationStatusList* list)
      : TrayItemMore(owner) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DRIVE).ToSkBitmap());
    Update(list);
  }

  virtual ~DriveDefaultView() {}

  void Update(const DriveOperationStatusList* list) {
    DCHECK(list);
    string16 label = GetTrayLabel(*list);
    SetLabel(label);
    SetAccessibleName(label);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveDefaultView);
};

class DriveDetailedView : public TrayDetailsView,
                          public ViewClickListener {
 public:
  DriveDetailedView(SystemTrayItem* owner,
                    const DriveOperationStatusList* list)
      : settings_(NULL),
        in_progress_img_(NULL),
        done_img_(NULL),
        failed_img_(NULL) {
    in_progress_img_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_AURA_UBER_TRAY_DRIVE);
    done_img_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_AURA_UBER_TRAY_DRIVE_DONE);
    failed_img_ = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_AURA_UBER_TRAY_DRIVE_FAILED);

    Update(list);
  }

  virtual ~DriveDetailedView() {
    STLDeleteValues(&update_map_);
  }

  void Update(const DriveOperationStatusList* list) {
    AppendOperationList(list);
    AppendSettings();
    AppendHeaderEntry(list);

    PreferredSizeChanged();
    SchedulePaint();
  }

 private:

  class OperationProgressBar : public views::ProgressBar {
   public:
    OperationProgressBar() {}
   private:

    // Overridden from View:
    virtual gfx::Size GetPreferredSize() OVERRIDE {
      return gfx::Size(kProgressBarWidth, kProgressBarHeight);
    }

    DISALLOW_COPY_AND_ASSIGN(OperationProgressBar);
  };

  class RowView : public HoverHighlightView,
                  public views::ButtonListener {
   public:
    RowView(DriveDetailedView* parent,
            ash::DriveOperationStatus::OperationState state,
            double progress,
            const FilePath& file_path)
        : HoverHighlightView(parent),
          container_(parent),
          status_img_(NULL),
          label_container_(NULL),
          progress_bar_(NULL),
          cancel_button_(NULL),
          file_path_(file_path) {
      // Status image.
      status_img_ = new views::ImageView();
      AddChildView(status_img_);

      label_container_ = new views::View();
      label_container_->SetLayoutManager(new views::BoxLayout(
          views::BoxLayout::kVertical, 0, 0, kVerticalPadding));
#if defined(OS_POSIX)
      string16 file_label =
          UTF8ToUTF16(file_path.BaseName().value());
#elif defined(OS_WIN)
      string16 file_label =
          WideToUTF16(file_path.BaseName().value());
#endif
      views::Label* label = new views::Label(file_label);
      label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      label_container_->AddChildView(label);
      // Add progress bar.
      progress_bar_ = new OperationProgressBar();
      label_container_->AddChildView(progress_bar_);

      AddChildView(label_container_);

      cancel_button_ = new views::ImageButton(this);
      cancel_button_->SetImage(views::ImageButton::BS_NORMAL,
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_AURA_UBER_TRAY_DRIVE_CANCEL));
      cancel_button_->SetImage(views::ImageButton::BS_HOT,
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_AURA_UBER_TRAY_DRIVE_CANCEL_HOVER));

      UpdateStatus(state, progress);
      AddChildView(cancel_button_);
    }

    void UpdateStatus(ash::DriveOperationStatus::OperationState state,
                      double progress) {
      status_img_->SetImage(container_->GetImageForState(state));
      progress_bar_->SetValue(progress);
      cancel_button_->SetVisible(
          state == ash::DriveOperationStatus::OPERATION_IN_PROGRESS ||
          state == ash::DriveOperationStatus::OPERATION_SUSPENDED);
    }

   private:

    // views::View overrides.
    virtual gfx::Size GetPreferredSize() OVERRIDE {
      return gfx::Size(
          status_img_->GetPreferredSize().width() +
          label_container_->GetPreferredSize().width() +
          cancel_button_->GetPreferredSize().width() +
              2 * kSidePadding + 2 * kHorizontalPadding,
          std::max(status_img_->GetPreferredSize().height(),
                   std::max(label_container_->GetPreferredSize().height(),
                            cancel_button_->GetPreferredSize().height())) +
                   kTopPadding + kBottomPadding);
    }

    virtual void Layout() OVERRIDE {
      gfx::Rect child_area(GetLocalBounds());
      if (child_area.IsEmpty())
        return;

      int pos_x = child_area.x() + kSidePadding;
      int pos_y = child_area.y() + kTopPadding;

      gfx::Rect bounds_status(
          gfx::Point(pos_x,
                     pos_y + (child_area.height() - kTopPadding -
                         kBottomPadding -
                         status_img_->GetPreferredSize().height())/2),
          status_img_->GetPreferredSize());
      status_img_->SetBoundsRect(bounds_status.Intersect(child_area));
      pos_x += status_img_->bounds().width() + kHorizontalPadding;

      gfx::Rect bounds_label(pos_x,
                             pos_y,
                             child_area.width() - 2 * kSidePadding -
                                 2 * kHorizontalPadding -
                                 status_img_->GetPreferredSize().width() -
                                 cancel_button_->GetPreferredSize().width(),
                             label_container_->GetPreferredSize().height());
      label_container_->SetBoundsRect(bounds_label.Intersect(child_area));
      pos_x += label_container_->bounds().width() + kHorizontalPadding;

      gfx::Rect bounds_button(
          gfx::Point(pos_x,
                     pos_y + (child_area.height() - kTopPadding -
                         kBottomPadding -
                         cancel_button_->GetPreferredSize().height())/2),
          cancel_button_->GetPreferredSize());
      cancel_button_->SetBoundsRect(bounds_button.Intersect(child_area));
    }

    // views::ButtonListener overrides.
    virtual void ButtonPressed(views::Button* sender,
                               const views::Event& event) OVERRIDE {
      DCHECK(sender == cancel_button_);
      container_->OnCancelOperation(file_path_);
    }

    DriveDetailedView* container_;
    views::ImageView* status_img_;
    views::View* label_container_;
    views::ProgressBar* progress_bar_;
    views::ImageButton* cancel_button_;
    FilePath file_path_;

    DISALLOW_COPY_AND_ASSIGN(RowView);
  };

  void AppendHeaderEntry(const DriveOperationStatusList* list) {
    if (footer())
      return;
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_DRIVE, this);
  }

  SkBitmap* GetImageForState(ash::DriveOperationStatus::OperationState state) {
    switch (state) {
      case ash::DriveOperationStatus::OPERATION_NOT_STARTED:
      case ash::DriveOperationStatus::OPERATION_STARTED:
      case ash::DriveOperationStatus::OPERATION_IN_PROGRESS:
      case ash::DriveOperationStatus::OPERATION_SUSPENDED:
        return in_progress_img_;
      case ash::DriveOperationStatus::OPERATION_COMPLETED:
        return done_img_;
      case ash::DriveOperationStatus::OPERATION_FAILED:
        return failed_img_;
    }
    return failed_img_;
  }

  virtual void OnCancelOperation(const FilePath& file_path) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    delegate->CancelDriveOperation(file_path);
  }

  void AppendOperationList(const DriveOperationStatusList* list) {
    if (!scroller())
      CreateScrollableList();

    // Apply the update.
    std::set<FilePath> new_set;
    for (DriveOperationStatusList::const_iterator it = list->begin();
         it != list->end(); ++it) {
      const DriveOperationStatus& operation = *it;

      new_set.insert(operation.file_path);
      std::map<FilePath, RowView*>::iterator existing_item =
          update_map_.find(operation.file_path);

      if (existing_item != update_map_.end()) {
        existing_item->second->UpdateStatus(operation.state,
                                            operation.progress);
      } else {
        RowView* row_view = new RowView(this,
                                        operation.state,
                                        operation.progress,
                                        operation.file_path);

        update_map_[operation.file_path] = row_view;
        scroll_content()->AddChildView(row_view);
      }
    }

    // Remove items from the list that haven't been added or modified with this
    // update batch.
    std::set<FilePath> remove_set;
    for (std::map<FilePath, RowView*>::iterator update_iter =
             update_map_.begin();
         update_iter != update_map_.end(); ++update_iter) {
      if (new_set.find(update_iter->first) == new_set.end()) {
        remove_set.insert(update_iter->first);
      }
    }

    for (std::set<FilePath>::iterator removed_iter = remove_set.begin();
        removed_iter != remove_set.end(); ++removed_iter)  {
      delete update_map_[*removed_iter];
      update_map_.erase(*removed_iter);
    }

    // Close the details if there is really nothing to show there anymore.
    if (new_set.empty())
      GetWidget()->Close();
  }

  void AppendSettings() {
    if (settings_)
      return;

    HoverHighlightView* container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddLabel(ui::ResourceBundle::GetSharedInstance().
        GetLocalizedString(IDS_ASH_STATUS_TRAY_DRIVE_SETTINGS),
        gfx::Font::NORMAL);
    AddChildView(container);
    settings_ = container;
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    if (sender == footer()->content()) {
      Shell::GetInstance()->tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
    } else if (sender == settings_) {
      delegate->ShowDriveSettings();
    }
  }

  // Maps operation entries to their file paths.
  std::map<FilePath, RowView*> update_map_;
  views::View* settings_;
  SkBitmap* in_progress_img_;
  SkBitmap* done_img_;
  SkBitmap* failed_img_;

  DISALLOW_COPY_AND_ASSIGN(DriveDetailedView);
};

}  // namespace tray

TrayDrive::TrayDrive() :
    TrayImageItem(IDR_AURA_UBER_TRAY_DRIVE_LIGHT),
    default_(NULL),
    detailed_(NULL) {
}

TrayDrive::~TrayDrive() {
}

bool TrayDrive::GetInitialVisibility() {
  scoped_ptr<DriveOperationStatusList> list(GetCurrentOperationList());
  return list->size() > 0;
}

views::View* TrayDrive::CreateDefaultView(user::LoginStatus status) {
  DCHECK(!default_);

  if (status != user::LOGGED_IN_USER && status != user::LOGGED_IN_OWNER)
    return NULL;

  scoped_ptr<DriveOperationStatusList> list(GetCurrentOperationList());
  if (!list->size())
    return NULL;

  default_ = new tray::DriveDefaultView(this, list.get());
  return default_;
}

views::View* TrayDrive::CreateDetailedView(user::LoginStatus status) {
  DCHECK(!detailed_);

  if (status != user::LOGGED_IN_USER && status != user::LOGGED_IN_OWNER)
    return NULL;

  scoped_ptr<DriveOperationStatusList> list(GetCurrentOperationList());
  if (!list->size())
    return NULL;

  detailed_ = new tray::DriveDetailedView(this, list.get());
  return detailed_;
}

void TrayDrive::DestroyDefaultView() {
  default_ = NULL;
}

void TrayDrive::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayDrive::UpdateAfterLoginStatusChange(user::LoginStatus status) {
  if (status == user::LOGGED_IN_USER || status == user::LOGGED_IN_OWNER)
    return;

  tray_view()->SetVisible(false);
  DestroyDefaultView();
  DestroyDetailedView();
}

void TrayDrive::OnDriveRefresh(const DriveOperationStatusList& list) {
  tray_view()->SetVisible(list.size() > 0);
  tray_view()->SchedulePaint();

  if (default_)
    default_->Update(&list);

  if (detailed_)
    detailed_->Update(&list);
}

}  // namespace internal
}  // namespace ash
