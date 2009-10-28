// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf.h"

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/dom_ui/downloads_ui.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

// DownloadShelfContextMenu ----------------------------------------------------

DownloadShelfContextMenu::DownloadShelfContextMenu(
    BaseDownloadItemModel* download_model)
    : download_(download_model->download()),
      model_(download_model) {
}

DownloadShelfContextMenu::~DownloadShelfContextMenu() {
}

bool DownloadShelfContextMenu::ItemIsChecked(int id) const {
  switch (id) {
    case OPEN_WHEN_COMPLETE: {
      return download_->open_when_complete();
    }
    case ALWAYS_OPEN_TYPE: {
      return download_->manager()->ShouldOpenFileBasedOnExtension(
          download_->full_path());
    }
    case TOGGLE_PAUSE: {
      return download_->is_paused();
    }
  }
  return false;
}

bool DownloadShelfContextMenu::ItemIsDefault(int id) const {
  return id == OPEN_WHEN_COMPLETE;
}

std::wstring DownloadShelfContextMenu::GetItemLabel(int id) const {
  switch (id) {
    case SHOW_IN_FOLDER:
      return l10n_util::GetString(IDS_DOWNLOAD_MENU_SHOW);
    case OPEN_WHEN_COMPLETE:
      if (download_->state() == DownloadItem::IN_PROGRESS)
        return l10n_util::GetString(IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE);
      return l10n_util::GetString(IDS_DOWNLOAD_MENU_OPEN);
    case ALWAYS_OPEN_TYPE:
      return l10n_util::GetString(IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE);
    case CANCEL:
      return l10n_util::GetString(IDS_DOWNLOAD_MENU_CANCEL);
    case TOGGLE_PAUSE: {
      if (download_->is_paused())
        return l10n_util::GetString(IDS_DOWNLOAD_MENU_RESUME_ITEM);
      else
        return l10n_util::GetString(IDS_DOWNLOAD_MENU_PAUSE_ITEM);
    }
    default:
      NOTREACHED();
  }
  return std::wstring();
}

bool DownloadShelfContextMenu::IsItemCommandEnabled(int id) const {
  switch (id) {
    case SHOW_IN_FOLDER:
    case OPEN_WHEN_COMPLETE:
      return download_->state() != DownloadItem::CANCELLED;
    case ALWAYS_OPEN_TYPE:
      return download_util::CanOpenDownload(download_);
    case CANCEL:
      return download_->state() == DownloadItem::IN_PROGRESS;
    case TOGGLE_PAUSE:
      return download_->state() == DownloadItem::IN_PROGRESS;
    default:
      return id > 0 && id < MENU_LAST;
  }
}

void DownloadShelfContextMenu::ExecuteItemCommand(int id) {
  switch (id) {
    case SHOW_IN_FOLDER:
      download_->manager()->ShowDownloadInShell(download_);
      break;
    case OPEN_WHEN_COMPLETE:
      download_util::OpenDownload(download_);
      break;
    case ALWAYS_OPEN_TYPE: {
      download_->manager()->OpenFilesBasedOnExtension(
          download_->full_path(), !ItemIsChecked(ALWAYS_OPEN_TYPE));
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
