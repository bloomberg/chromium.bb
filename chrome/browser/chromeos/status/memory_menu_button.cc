// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: this file is used by Aura on all linux platforms, even though it
// is currently in a chromeos specific location.

#include "chrome/browser/chromeos/status/memory_menu_button.h"

#include "base/file_util.h"
#include "base/process_util.h"  // GetSystemMemoryInfo
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/memory_purger.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/google/heap-profiler.h"
#endif

#if defined(USE_TCMALLOC)
const char kProfileDumpFilePrefix[] = "/tmp/chrome_tcmalloc";
#endif

namespace {

// views::MenuItemView item ids
enum {
  MEM_TOTAL_ITEM,
  MEM_FREE_ITEM,
  MEM_BUFFERS_ITEM,
  MEM_CACHE_ITEM,
  SHMEM_ITEM,
  PURGE_MEMORY_ITEM,
#if defined(USE_TCMALLOC)
  TOGGLE_PROFILING_ITEM,
  DUMP_PROFILING_ITEM,
#endif
};

}  // namespace

// Delay between updates, in seconds.
const int kUpdateIntervalSeconds = 5;

MemoryMenuButton::MemoryMenuButton(StatusAreaButton::Delegate* delegate)
    : StatusAreaButton(delegate, this),
      meminfo_(new base::SystemMemoryInfoKB()),
      renderer_kills_(0) {
  set_id(VIEW_ID_STATUS_BUTTON_MEMORY);
  // Track renderer kills, as the kernel OOM killer will start to kill our
  // renderers as we run out of memory.
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  UpdateTextAndSetNextTimer();
}

MemoryMenuButton::~MemoryMenuButton() {
}

void MemoryMenuButton::UpdateTextAndSetNextTimer() {
  UpdateText();

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kUpdateIntervalSeconds), this,
               &MemoryMenuButton::UpdateTextAndSetNextTimer);
}

void MemoryMenuButton::UpdateText() {
  base::GetSystemMemoryInfo(meminfo_.get());
  // "Anonymous" memory, meaning not mapped to a file (which has a name),
  // represents memory that has been dynamically allocated to a process.
  // It thus approximates heap memory usage across all processes.
  int anon_kb = meminfo_->active_anon + meminfo_->inactive_anon;
  std::string label = base::StringPrintf("%d MB (%d)",
                                         anon_kb / 1024,
                                         renderer_kills_);
  SetText(ASCIIToUTF16(label));
  std::string tooltip = base::StringPrintf("%d MB allocated (anonymous)\n"
                                           "%d renderer kill(s)",
                                           anon_kb / 1024,
                                           renderer_kills_);
  SetTooltipText(ASCIIToUTF16(tooltip));
  SchedulePaint();
}

// MemoryMenuButton, views::MenuDelegate implementation:
string16 MemoryMenuButton::GetLabel(int id) const {
  std::string label;
  switch (id) {
    case MEM_TOTAL_ITEM:
      label = base::StringPrintf("%d MB total", meminfo_->total / 1024);
      break;
    case MEM_FREE_ITEM:
      label = base::StringPrintf("%d MB free", meminfo_->free / 1024);
      break;
    case MEM_BUFFERS_ITEM:
      label = base::StringPrintf("%d MB buffers", meminfo_->buffers / 1024);
      break;
    case MEM_CACHE_ITEM:
      label = base::StringPrintf("%d MB cache", meminfo_->cached / 1024);
      break;
    case SHMEM_ITEM:
      label = base::StringPrintf("%d MB shmem", meminfo_->shmem / 1024);
      break;
    case PURGE_MEMORY_ITEM:
      return ASCIIToUTF16("Purge memory");
#if defined(USE_TCMALLOC)
    case TOGGLE_PROFILING_ITEM:
      if (!IsHeapProfilerRunning())
        return ASCIIToUTF16("Start profiling");
      else
        return ASCIIToUTF16("Stop profiling");
    case DUMP_PROFILING_ITEM:
        return ASCIIToUTF16("Dump profile");
#endif
    default:
      return string16();
  }
  return UTF8ToUTF16(label);
}

bool MemoryMenuButton::IsCommandEnabled(int id) const {
  switch (id) {
    case PURGE_MEMORY_ITEM:
      return true;
#if defined(USE_TCMALLOC)
    case TOGGLE_PROFILING_ITEM:
    case DUMP_PROFILING_ITEM:
      return true;
#endif
    default:
      return false;
  }
}

namespace {
#if defined(USE_TCMALLOC)
FilePath::StringType GetProfileDumpFilePath(base::ProcessId pid) {
  int int_pid = static_cast<int>(pid);
  FilePath::StringType filepath = StringPrintf(
      FILE_PATH_LITERAL("%s.%d.heap"),
      FILE_PATH_LITERAL(kProfileDumpFilePrefix), int_pid);
  return filepath;
}
#endif
}

void MemoryMenuButton::SendCommandToRenderers(int id) {
#if defined(USE_TCMALLOC)
  // Use the "is running" value for this process to determine whether to
  // start or stop profiling on the renderer processes.
  bool started = IsHeapProfilerRunning();
  for (content::RenderProcessHost::iterator it =
          content::RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    switch (id) {
      case TOGGLE_PROFILING_ITEM:
        it.GetCurrentValue()->Send(new ChromeViewMsg_SetTcmallocHeapProfiling(
            started, std::string(kProfileDumpFilePrefix)));
        break;
      case DUMP_PROFILING_ITEM:
        it.GetCurrentValue()->Send(new ChromeViewMsg_WriteTcmallocHeapProfile(
            GetProfileDumpFilePath(
                base::GetProcId(it.GetCurrentValue()->GetHandle()))));
        break;
      default:
        NOTREACHED();
    }
  }
#endif
}

void MemoryMenuButton::ExecuteCommand(int id) {
  switch (id) {
    case PURGE_MEMORY_ITEM:
      MemoryPurger::PurgeAll();
      break;
#if defined(USE_TCMALLOC)
    case TOGGLE_PROFILING_ITEM: {
      if (!IsHeapProfilerRunning())
        HeapProfilerStart(kProfileDumpFilePrefix);
      else
        HeapProfilerStop();
      SendCommandToRenderers(id);
      break;
    }
    case DUMP_PROFILING_ITEM: {
      char* profile = GetHeapProfile();
      if (profile) {
        FilePath::StringType filepath =
            GetProfileDumpFilePath(base::GetProcId(base::GetCurrentProcId()));
        VLOG(0) << "Writing browser heap profile dump to: " << filepath;
        base::ThreadRestrictions::ScopedAllowIO allow_io;
        file_util::WriteFile(FilePath(filepath), profile, strlen(profile));
        delete profile;
      }
      SendCommandToRenderers(id);
      break;
    }
#endif
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

  menu_runner_.reset(new views::MenuRunner(CreateMenu()));
  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  if (menu_runner_->RunMenuAt(
          source->GetWidget()->GetTopLevelWidget(), this, bounds,
          views::MenuItemView::TOPRIGHT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
}

views::MenuItemView* MemoryMenuButton::CreateMenu() {
  // Just rebuild the menu each time to ensure the labels are up-to-date.
  views::MenuItemView* menu = new views::MenuItemView(this);
  // Text for these items will be set by GetLabel().
  menu->AppendDelegateMenuItem(MEM_TOTAL_ITEM);
  menu->AppendDelegateMenuItem(MEM_FREE_ITEM);
  menu->AppendDelegateMenuItem(MEM_BUFFERS_ITEM);
  menu->AppendDelegateMenuItem(MEM_CACHE_ITEM);
  menu->AppendDelegateMenuItem(SHMEM_ITEM);
  menu->AppendSeparator();
  menu->AppendDelegateMenuItem(PURGE_MEMORY_ITEM);
#if defined(USE_TCMALLOC)
  menu->AppendSeparator();
  menu->AppendDelegateMenuItem(TOGGLE_PROFILING_ITEM);
  if (IsHeapProfilerRunning())
    menu->AppendDelegateMenuItem(DUMP_PROFILING_ITEM);
#endif
  return menu;
}

/////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver overrides.

void MemoryMenuButton::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost::RendererClosedDetails* process_details =
          content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details).ptr();
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
