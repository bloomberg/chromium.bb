// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf.h"

#include "base/file_util.h"
#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// DownloadShelfContextMenu ----------------------------------------------------

DownloadShelfContextMenu::DownloadShelfContextMenu(
    BaseDownloadItemModel* download_model)
    : download_(download_model->download()),
      model_(download_model) {
}

DownloadShelfContextMenu::~DownloadShelfContextMenu() {
}

DownloadItem* DownloadShelfContextMenu::download() const {
  return download_;
}

bool DownloadShelfContextMenu::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case OPEN_WHEN_COMPLETE:
      return download_->open_when_complete();
    case ALWAYS_OPEN_TYPE:
      return download_->ShouldOpenFileBasedOnExtension();
  }
  return false;
}

string16 DownloadShelfContextMenu::GetLabelForCommandId(int command_id) const {
  switch (command_id) {
    case SHOW_IN_FOLDER:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_SHOW);
    case OPEN_WHEN_COMPLETE:
      if (download_->state() == DownloadItem::IN_PROGRESS)
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE);
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_OPEN);
    case ALWAYS_OPEN_TYPE:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE);
    case CANCEL:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_CANCEL);
    case TOGGLE_PAUSE: {
      if (download_->is_paused())
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_RESUME_ITEM);
      else
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_PAUSE_ITEM);
    }
    default:
      NOTREACHED();
  }
  return string16();
}

bool DownloadShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case SHOW_IN_FOLDER:
    case OPEN_WHEN_COMPLETE:
      return download_->state() != DownloadItem::CANCELLED;
    case ALWAYS_OPEN_TYPE:
      return download_->CanOpenDownload();
    case CANCEL:
      return download_->state() == DownloadItem::IN_PROGRESS;
    case TOGGLE_PAUSE:
      return download_->state() == DownloadItem::IN_PROGRESS;
    default:
      return command_id > 0 && command_id < MENU_LAST;
  }
}

void DownloadShelfContextMenu::ExecuteCommand(int command_id) {
  switch (command_id) {
    case SHOW_IN_FOLDER:
      download_->ShowDownloadInShell();
      break;
    case OPEN_WHEN_COMPLETE:
      download_->OpenDownload();
      break;
    case ALWAYS_OPEN_TYPE: {
      download_->OpenFilesBasedOnExtension(
          !IsCommandIdChecked(ALWAYS_OPEN_TYPE));
      break;
    }
    case CANCEL:
      model_->CancelTask();
      break;
    case TOGGLE_PAUSE:
      // It is possible for the download to complete before the user clicks the
      // menu item, recheck if the download is in progress state before toggling
      // pause.
      if (download_->state() == DownloadItem::IN_PROGRESS)
        download_->TogglePause();
      break;
    default:
      NOTREACHED();
  }
}

bool DownloadShelfContextMenu::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  return false;
}

bool DownloadShelfContextMenu::IsItemForCommandIdDynamic(
    int command_id) const {
  return command_id == TOGGLE_PAUSE;
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetInProgressMenuModel() {
  if (in_progress_download_menu_model_.get())
    return in_progress_download_menu_model_.get();

  in_progress_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  in_progress_download_menu_model_->AddCheckItemWithStringId(
      OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE);
  in_progress_download_menu_model_->AddCheckItemWithStringId(
      ALWAYS_OPEN_TYPE, IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE);
  in_progress_download_menu_model_->AddSeparator();
  in_progress_download_menu_model_->AddItemWithStringId(
      TOGGLE_PAUSE, IDS_DOWNLOAD_MENU_PAUSE_ITEM);
  in_progress_download_menu_model_->AddItemWithStringId(
      SHOW_IN_FOLDER, IDS_DOWNLOAD_MENU_SHOW);
  in_progress_download_menu_model_->AddSeparator();
  in_progress_download_menu_model_->AddItemWithStringId(
      CANCEL, IDS_DOWNLOAD_MENU_CANCEL);

  return in_progress_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetFinishedMenuModel() {
  if (finished_download_menu_model_.get())
    return finished_download_menu_model_.get();

  finished_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  finished_download_menu_model_->AddItemWithStringId(
      OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN);
  finished_download_menu_model_->AddCheckItemWithStringId(
      ALWAYS_OPEN_TYPE, IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE);
  finished_download_menu_model_->AddSeparator();
  finished_download_menu_model_->AddItemWithStringId(
      SHOW_IN_FOLDER, IDS_DOWNLOAD_MENU_SHOW);
  finished_download_menu_model_->AddSeparator();
  finished_download_menu_model_->AddItemWithStringId(
      CANCEL, IDS_DOWNLOAD_MENU_CANCEL);

  return finished_download_menu_model_.get();
}
