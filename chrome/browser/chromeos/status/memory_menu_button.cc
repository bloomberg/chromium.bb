// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/memory_menu_button.h"

#include "base/file_util.h"
#include "base/process_util.h"  // GetSystemMemoryInfo
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/memory_purger.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/widget/widget.h"

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
#if defined(USE_TCMALLOC)
    case TOGGLE_PROFILING_ITEM:
      if (!IsHeapProfilerRunning())
        return L"Start profiling";
      else
        return L"Stop profiling";
    case DUMP_PROFILING_ITEM:
        return L"Dump profile";
#endif
    default:
      return std::wstring();
  }
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
FilePath::StringType GetFilePath(base::ProcessId pid) {
  int int_pid = static_cast<int>(pid);
  FilePath::StringType filepath = StringPrintf(
      FILE_PATH_LITERAL("%s.%d.heap"),
      FILE_PATH_LITERAL(kProfileDumpFilePrefix), int_pid);
  return filepath;
}
}

void MemoryMenuButton::SendCommandToRenderers(int id) {
#if defined(USE_TCMALLOC)
  // Use the "is running" value for this process to determine whether to
  // start or stop profiling on the renderer processes.
  bool started = IsHeapProfilerRunning();
  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    switch (id) {
      case TOGGLE_PROFILING_ITEM:
        it.GetCurrentValue()->Send(new ViewMsg_SetTcmallocHeapProfiling(
            started, std::string(kProfileDumpFilePrefix)));
        break;
      case DUMP_PROFILING_ITEM:
        it.GetCurrentValue()->Send(new ViewMsg_WriteTcmallocHeapProfile(
            GetFilePath(base::GetProcId(it.GetCurrentValue()->GetHandle()))));
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
            GetFilePath(base::GetProcId(base::GetCurrentProcId()));
        VLOG(0) << "Writing browser heap profile dump to: " << filepath;
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
  menu_->AppendSeparator();
  menu_->AppendDelegateMenuItem(PURGE_MEMORY_ITEM);
#if defined(USE_TCMALLOC)
  menu_->AppendSeparator();
  menu_->AppendDelegateMenuItem(TOGGLE_PROFILING_ITEM);
  if (IsHeapProfilerRunning())
    menu_->AppendDelegateMenuItem(DUMP_PROFILING_ITEM);
#endif
}

}  // namespace chromeos
