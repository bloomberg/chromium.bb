// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/memory_menu_button.h"

#include "base/process_util.h"  // GetSystemMemoryInfo
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/memory_purger.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/widget/widget.h"

namespace {

// views::MenuItemView item ids
enum {
  MEM_TOTAL_ITEM,
  MEM_FREE_ITEM,
  MEM_BUFFERS_ITEM,
  MEM_CACHE_ITEM,
  SHMEM_ITEM,
  PURGE_MEMORY_ITEM,
};

}  // namespace

namespace chromeos {

// Delay between updates, in seconds.
const int kUpdateIntervalSeconds = 5;

MemoryMenuButton::MemoryMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this),
      mem_total_(0),
      shmem_(0),
      mem_free_(0),
      mem_buffers_(0),
      mem_cache_(0) {
  UpdateTextAndSetNextTimer();
}

MemoryMenuButton::~MemoryMenuButton() {
}

void MemoryMenuButton::UpdateTextAndSetNextTimer() {
  UpdateText();

  timer_.Start(base::TimeDelta::FromSeconds(kUpdateIntervalSeconds), this,
               &MemoryMenuButton::UpdateTextAndSetNextTimer);
}

void MemoryMenuButton::UpdateText() {
  base::GetSystemMemoryInfo(&mem_total_, &mem_free_, &mem_buffers_, &mem_cache_,
                            &shmem_);
  std::wstring label = base::StringPrintf(L"%d MB", mem_free_ / 1024);
  SetText(label);
  std::wstring tooltip = base::StringPrintf(
      L"%d MB total\n%d MB free\n%d MB buffers\n%d MB cache\n%d MB shmem",
      mem_total_ / 1024,
      mem_free_ / 1024,
      mem_buffers_ / 1024,
      mem_cache_ / 1024,
      shmem_ / 1024);
  SetTooltipText(tooltip);
  SchedulePaint();
}

// MemoryMenuButton, views::MenuDelegate implementation:
std::wstring MemoryMenuButton::GetLabel(int id) const {
  switch (id) {
    case MEM_TOTAL_ITEM:
      return StringPrintf(L"%d MB total", mem_total_ / 1024);
    case MEM_FREE_ITEM:
      return StringPrintf(L"%d MB free", mem_free_ / 1024);
    case MEM_BUFFERS_ITEM:
      return StringPrintf(L"%d MB buffers", mem_buffers_ / 1024);
    case MEM_CACHE_ITEM:
      return StringPrintf(L"%d MB cache", mem_cache_ / 1024);
    case SHMEM_ITEM:
      return StringPrintf(L"%d MB shmem", shmem_ / 1024);
    case PURGE_MEMORY_ITEM:
      return L"Purge memory";
    default:
      return std::wstring();
  }
}

bool MemoryMenuButton::IsCommandEnabled(int id) const {
  switch (id) {
    case PURGE_MEMORY_ITEM:
      return true;
    default:
      return false;
  }
}

void MemoryMenuButton::ExecuteCommand(int id) {
  switch (id) {
    case PURGE_MEMORY_ITEM:
      MemoryPurger::PurgeAll();
      break;
    default:
      NOTREACHED();
      break;
  }
}

int MemoryMenuButton::horizontal_padding() {
  return 3;
}

// MemoryMenuButton, views::ViewMenuDelegate implementation:

void MemoryMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  // View passed in must be a views::MenuButton, i.e. the MemoryMenuButton.
  DCHECK_EQ(source, this);

  EnsureMenu();

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  menu_->RunMenuAt(
      source->GetWidget()->GetTopLevelWidget(),
      this,
      bounds,
      views::MenuItemView::TOPRIGHT,
      true);
}

// MemoryMenuButton, views::View implementation:

void MemoryMenuButton::EnsureMenu() {
  // Just rebuild the menu each time to ensure the labels are up-to-date.
  menu_.reset(new views::MenuItemView(this));
  // Text for these items will be set by GetLabel().
  menu_->AppendDelegateMenuItem(MEM_TOTAL_ITEM);
  menu_->AppendDelegateMenuItem(MEM_FREE_ITEM);
  menu_->AppendDelegateMenuItem(MEM_BUFFERS_ITEM);
  menu_->AppendDelegateMenuItem(MEM_CACHE_ITEM);
  menu_->AppendDelegateMenuItem(SHMEM_ITEM);
  // TODO(jamescook): Dump heap profiles?
  menu_->AppendSeparator();
  menu_->AppendDelegateMenuItem(PURGE_MEMORY_ITEM);
}

}  // namespace chromeos
