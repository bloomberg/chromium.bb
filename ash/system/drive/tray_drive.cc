// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/drive/tray_drive.h"

#include <vector>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const int kSidePadding = 8;
const int kHorizontalPadding = 6;
const int kVerticalPadding = 6;
const int kTopPadding = 6;
const int kBottomPadding = 10;
const int kProgressBarWidth = 100;
const int kProgressBarHeight = 11;
const int64 kHideDelayInMs = 1000;

base::string16 GetTrayLabel(const ash::DriveOperationStatusList& list) {
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DRIVE_SYNCING,
      base::IntToString16(static_cast<int>(list.size())));
}

scoped_ptr<ash::DriveOperationStatusList> GetCurrentOperationList() {
  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  scoped_ptr<ash::DriveOperationStatusList> list(
      new ash::DriveOperationStatusList);
  delegate->GetDriveOperationStatusList(list.get());
  return list.Pass();
}

}

namespace tray {

class DriveDefaultView : public TrayItemMore {
 public:
  DriveDefaultView(SystemTrayItem* owner,
                   const DriveOperationStatusList* list)
      : TrayItemMore(owner, true) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DRIVE).ToImageSkia());
    Update(list);
  }

  virtual ~DriveDefaultView() {}

  void Update(const DriveOperationStatusList* list) {
    DCHECK(list);
    base::string16 label = GetTrayLabel(*list);
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
      : TrayDetailsView(owner),
        settings_(NULL),
        in_progress_img_(NULL),
        done_img_(NULL),
        failed_img_(NULL) {
    in_progress_img_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_DRIVE);
    done_img_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_AURA_UBER_TRAY_DRIVE_DONE);
    failed_img_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
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

    SchedulePaint();
  }

 private:

  class OperationProgressBar : public views::ProgressBar {
   public:
    OperationProgressBar() {}
   private:

    // Overridden from View:
    virtual gfx::Size GetPreferredSize() const OVERRIDE {
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
            const base::FilePath& file_path,
            int32 operation_id)
        : HoverHighlightView(parent),
          container_(parent),
          status_img_(NULL),
          label_container_(NULL),
          progress_bar_(NULL),
          cancel_button_(NULL),
          operation_id_(operation_id) {
      // Status image.
      status_img_ = new views::ImageView();
      AddChildView(status_img_);

      label_container_ = new views::View();
      label_container_->SetLayoutManager(new views::BoxLayout(
          views::BoxLayout::kVertical, 0, 0, kVerticalPadding));
#if defined(OS_POSIX)
      base::string16 file_label =
          base::UTF8ToUTF16(file_path.BaseName().value());
#elif defined(OS_WIN)
      base::string16 file_label =
          base::WideToUTF16(file_path.BaseName().value());
#endif
      views::Label* label = new views::Label(file_label);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      label_container_->AddChildView(label);
      // Add progress bar.
      progress_bar_ = new OperationProgressBar();
      label_container_->AddChildView(progress_bar_);

      AddChildView(label_container_);

      cancel_button_ = new views::ImageButton(this);
      cancel_button_->SetImage(views::ImageButton::STATE_NORMAL,
          ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AURA_UBER_TRAY_DRIVE_CANCEL));
      cancel_button_->SetImage(views::ImageButton::STATE_HOVERED,
          ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AURA_UBER_TRAY_DRIVE_CANCEL_HOVER));

      UpdateStatus(state, progress);
      AddChildView(cancel_button_);
    }

    void UpdateStatus(ash::DriveOperationStatus::OperationState state,
                      double progress) {
      status_img_->SetImage(container_->GetImageForState(state));
      progress_bar_->SetValue(progress);
      cancel_button_->SetVisible(
          state == ash::DriveOperationStatus::OPERATION_NOT_STARTED ||
          state == ash::DriveOperationStatus::OPERATION_IN_PROGRESS);
    }

   private:

    // views::View overrides.
    virtual gfx::Size GetPreferredSize() const OVERRIDE {
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
      status_img_->SetBoundsRect(
          gfx::IntersectRects(bounds_status, child_area));
      pos_x += status_img_->bounds().width() + kHorizontalPadding;

      gfx::Rect bounds_label(pos_x,
                             pos_y,
                             child_area.width() - 2 * kSidePadding -
                                 2 * kHorizontalPadding -
                                 status_img_->GetPreferredSize().width() -
                                 cancel_button_->GetPreferredSize().width(),
                             label_container_->GetPreferredSize().height());
      label_container_->SetBoundsRect(
          gfx::IntersectRects(bounds_label, child_area));
      pos_x += label_container_->bounds().width() + kHorizontalPadding;

      gfx::Rect bounds_button(
          gfx::Point(pos_x,
                     pos_y + (child_area.height() - kTopPadding -
                         kBottomPadding -
                         cancel_button_->GetPreferredSize().height())/2),
          cancel_button_->GetPreferredSize());
      cancel_button_->SetBoundsRect(
          gfx::IntersectRects(bounds_button, child_area));
    }

    // views::ButtonListener overrides.
    virtual void ButtonPressed(views::Button* sender,
                               const ui::Event& event) OVERRIDE {
      DCHECK(sender == cancel_button_);
      Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          ash::UMA_STATUS_AREA_DRIVE_CANCEL_OPERATION);
      container_->OnCancelOperation(operation_id_);
    }

    DriveDetailedView* container_;
    views::ImageView* status_img_;
    views::View* label_container_;
    views::ProgressBar* progress_bar_;
    views::ImageButton* cancel_button_;
    int32 operation_id_;

    DISALLOW_COPY_AND_ASSIGN(RowView);
  };

  void AppendHeaderEntry(const DriveOperationStatusList* list) {
    if (footer())
      return;
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_DRIVE, this);
  }

  gfx::ImageSkia* GetImageForState(
      ash::DriveOperationStatus::OperationState state) {
    switch (state) {
      case ash::DriveOperationStatus::OPERATION_NOT_STARTED:
      case ash::DriveOperationStatus::OPERATION_IN_PROGRESS:
        return in_progress_img_;
      case ash::DriveOperationStatus::OPERATION_COMPLETED:
        return done_img_;
      case ash::DriveOperationStatus::OPERATION_FAILED:
        return failed_img_;
    }
    return failed_img_;
  }

  void OnCancelOperation(int32 operation_id) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    delegate->CancelDriveOperation(operation_id);
  }

  void AppendOperationList(const DriveOperationStatusList* list) {
    if (!scroller())
      CreateScrollableList();

    // Apply the update.
    std::set<base::FilePath> new_set;
    bool item_list_changed = false;
    for (DriveOperationStatusList::const_iterator it = list->begin();
         it != list->end(); ++it) {
      const DriveOperationStatus& operation = *it;

      new_set.insert(operation.file_path);
      std::map<base::FilePath, RowView*>::iterator existing_item =
          update_map_.find(operation.file_path);

      if (existing_item != update_map_.end()) {
        existing_item->second->UpdateStatus(operation.state,
                                            operation.progress);
      } else {
        RowView* row_view = new RowView(this,
                                        operation.state,
                                        operation.progress,
                                        operation.file_path,
                                        operation.id);

        update_map_[operation.file_path] = row_view;
        scroll_content()->AddChildView(row_view);
        item_list_changed = true;
      }
    }

    // Remove items from the list that haven't been added or modified with this
    // update batch.
    std::set<base::FilePath> remove_set;
    for (std::map<base::FilePath, RowView*>::iterator update_iter =
             update_map_.begin();
         update_iter != update_map_.end(); ++update_iter) {
      if (new_set.find(update_iter->first) == new_set.end()) {
        remove_set.insert(update_iter->first);
      }
    }

    for (std::set<base::FilePath>::iterator removed_iter = remove_set.begin();
        removed_iter != remove_set.end(); ++removed_iter)  {
      delete update_map_[*removed_iter];
      update_map_.erase(*removed_iter);
      item_list_changed = true;
    }

    if (item_list_changed)
      scroller()->Layout();

    // Close the details if there is really nothing to show there anymore.
    if (new_set.empty() && GetWidget())
      GetWidget()->Close();
  }

  void AppendSettings() {
    if (settings_)
      return;

    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddLabel(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_DRIVE_SETTINGS),
        gfx::ALIGN_LEFT,
        gfx::Font::NORMAL);
    AddChildView(container);
    settings_ = container;
  }

  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    if (sender == footer()->content()) {
      TransitionToDefaultView();
    } else if (sender == settings_) {
      delegate->ShowDriveSettings();
    }
  }

  // Maps operation entries to their file paths.
  std::map<base::FilePath, RowView*> update_map_;
  views::View* settings_;
  gfx::ImageSkia* in_progress_img_;
  gfx::ImageSkia* done_img_;
  gfx::ImageSkia* failed_img_;

  DISALLOW_COPY_AND_ASSIGN(DriveDetailedView);
};

}  // namespace tray

TrayDrive::TrayDrive(SystemTray* system_tray) :
    TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_DRIVE_LIGHT),
    default_(NULL),
    detailed_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->AddDriveObserver(this);
}

TrayDrive::~TrayDrive() {
  Shell::GetInstance()->system_tray_notifier()->RemoveDriveObserver(this);
}

bool TrayDrive::GetInitialVisibility() {
  return false;
}

views::View* TrayDrive::CreateDefaultView(user::LoginStatus status) {
  DCHECK(!default_);

  if (status != user::LOGGED_IN_USER && status != user::LOGGED_IN_OWNER)
    return NULL;

  // If the list is empty AND the tray icon is invisible (= not in the margin
  // duration of delayed item hiding), don't show the item.
  scoped_ptr<DriveOperationStatusList> list(GetCurrentOperationList());
  if (list->empty() && !tray_view()->visible())
    return NULL;

  default_ = new tray::DriveDefaultView(this, list.get());
  return default_;
}

views::View* TrayDrive::CreateDetailedView(user::LoginStatus status) {
  DCHECK(!detailed_);

  if (status != user::LOGGED_IN_USER && status != user::LOGGED_IN_OWNER)
    return NULL;

  // If the list is empty AND the tray icon is invisible (= not in the margin
  // duration of delayed item hiding), don't show the item.
  scoped_ptr<DriveOperationStatusList> list(GetCurrentOperationList());
  if (list->empty() && !tray_view()->visible())
    return NULL;

  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      ash::UMA_STATUS_AREA_DETAILED_DRIVE_VIEW);
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

void TrayDrive::OnDriveJobUpdated(const DriveOperationStatus& status) {
  // The Drive job list manager changed its notification interface *not* to send
  // the whole list of operations each time, to clarify which operation is
  // updated and to reduce redundancy.
  //
  // TrayDrive should be able to benefit from the change, but for now, to
  // incrementally migrate to the new way with minimum diffs, we still get the
  // list of operations each time the event is fired.
  // TODO(kinaba) http://crbug.com/128079 clean it up.
  scoped_ptr<DriveOperationStatusList> list(GetCurrentOperationList());
  bool is_new_item = true;
  for (size_t i = 0; i < list->size(); ++i) {
    if ((*list)[i].id == status.id) {
      (*list)[i] = status;
      is_new_item = false;
      break;
    }
  }
  if (is_new_item)
    list->push_back(status);

  // Check if all the operations are in the finished state.
  bool all_jobs_finished = true;
  for (size_t i = 0; i < list->size(); ++i) {
    if ((*list)[i].state != DriveOperationStatus::OPERATION_COMPLETED &&
        (*list)[i].state != DriveOperationStatus::OPERATION_FAILED) {
      all_jobs_finished = false;
      break;
    }
  }

  if (all_jobs_finished) {
    // If all the jobs ended, the tray item will be hidden after a certain
    // amount of delay. This is to avoid flashes between sequentially executed
    // Drive operations (see crbug/165679).
    hide_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(kHideDelayInMs),
                      this,
                      &TrayDrive::HideIfNoOperations);
    return;
  }

  // If the list is non-empty, stop the hiding timer (if any).
  hide_timer_.Stop();

  tray_view()->SetVisible(true);
  if (default_)
    default_->Update(list.get());
  if (detailed_)
    detailed_->Update(list.get());
}

void TrayDrive::HideIfNoOperations() {
  DriveOperationStatusList empty_list;

  tray_view()->SetVisible(false);
  if (default_)
    default_->Update(&empty_list);
  if (detailed_)
    detailed_->Update(&empty_list);
}

}  // namespace ash
