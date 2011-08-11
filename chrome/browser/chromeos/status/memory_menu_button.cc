// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/memory_menu_button.h"

#include "base/process_util.h"  // GetSystemMemoryInfo
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/memory_purger.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/common/notification_service.h"
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
      meminfo_(new base::SystemMemoryInfoKB()),
      renderer_kills_(0) {
  // Track renderer kills, as the kernel OOM killer will start to kill our
  // renderers as we run out of memory.
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
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
  base::GetSystemMemoryInfo(meminfo_.get());
  // "Anonymous" memory, meaning not mapped to a file (which has a name),
  // represents memory that has been dynamically allocated to a process.
  // It thus approximates heap memory usage across all processes.
  int anon_kb = meminfo_->active_anon + meminfo_->inactive_anon;
  std::wstring label = base::StringPrintf(L"%d MB (%d)",
                                          anon_kb / 1024,
                                          renderer_kills_);
  SetText(label);
  std::wstring tooltip = base::StringPrintf(
      L"%d MB allocated (anonymous)\n"
      L"%d renderer kill(s)",
      anon_kb / 1024,
      renderer_kills_);
  SetTooltipText(tooltip);
  SchedulePaint();
}

// MemoryMenuButton, views::MenuDelegate implementation:
std::wstring MemoryMenuButton::GetLabel(int id) const {
  switch (id) {
    case MEM_TOTAL_ITEM:
      return StringPrintf(L"%d MB total", meminfo_->total / 1024);
    case MEM_FREE_ITEM:
      return StringPrintf(L"%d MB free", meminfo_->free / 1024);
    case MEM_BUFFERS_ITEM:
      return StringPrintf(L"%d MB buffers", meminfo_->buffers / 1024);
    case MEM_CACHE_ITEM:
      return StringPrintf(L"%d MB cache", meminfo_->cached / 1024);
    case SHMEM_ITEM:
      return StringPrintf(L"%d MB shmem", meminfo_->shmem / 1024);
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

/////////////////////////////////////////////////////////////////////////////
// NotificationObserver overrides.

void MemoryMenuButton::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      RenderProcessHost::RendererClosedDetails* process_details =
          Details<RenderProcessHost::RendererClosedDetails>(details).ptr();
      if (process_details->status ==
          base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
        renderer_kills_++;
        // A kill is a very interesting event, so repaint immediately.
        UpdateText();
      }
      break;
    }
    default:
      NOTREACHED() << L"Received unexpected notification";
      break;
  }
}

}  // namespace chromeos
