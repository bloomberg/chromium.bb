// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/desktop_media_list_ash.h"

#include <set>

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media/desktop_media_list_observer.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "media/base/video_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

using content::BrowserThread;
using content::DesktopMediaID;

namespace {

// Update the list twice per second.
const int kDefaultUpdatePeriod = 500;

}  // namespace

DesktopMediaListAsh::SourceDescription::SourceDescription(
    DesktopMediaID id,
    const base::string16& name)
    : id(id),
      name(name) {
}

DesktopMediaListAsh::DesktopMediaListAsh(int source_types)
    : source_types_(source_types),
      update_period_(base::TimeDelta::FromMilliseconds(kDefaultUpdatePeriod)),
      thumbnail_size_(100, 100),
      view_dialog_id_(-1),
      observer_(NULL),
      pending_window_capture_requests_(0),
      weak_factory_(this) {
}

DesktopMediaListAsh::~DesktopMediaListAsh() {}

void DesktopMediaListAsh::SetUpdatePeriod(base::TimeDelta period) {
  DCHECK(!observer_);
  update_period_ = period;
}

void DesktopMediaListAsh::SetThumbnailSize(
    const gfx::Size& thumbnail_size) {
  thumbnail_size_ = thumbnail_size;
}

void DesktopMediaListAsh::SetViewDialogWindowId(
    content::DesktopMediaID::Id dialog_id) {
  view_dialog_id_ = dialog_id;
}

void DesktopMediaListAsh::StartUpdating(DesktopMediaListObserver* observer) {
  DCHECK(!observer_);

  observer_ = observer;
  Refresh();
}

int DesktopMediaListAsh::GetSourceCount() const {
  return sources_.size();
}

const DesktopMediaList::Source& DesktopMediaListAsh::GetSource(
    int index) const {
  return sources_[index];
}

void DesktopMediaListAsh::Refresh() {
  std::vector<SourceDescription> new_sources;
  EnumerateSources(&new_sources);

  typedef std::set<content::DesktopMediaID> SourceSet;
  SourceSet new_source_set;
  for (size_t i = 0; i < new_sources.size(); ++i) {
    new_source_set.insert(new_sources[i].id);
  }
  // Iterate through the old sources to find the removed sources.
  for (size_t i = 0; i < sources_.size(); ++i) {
    if (new_source_set.find(sources_[i].id) == new_source_set.end()) {
      sources_.erase(sources_.begin() + i);
      observer_->OnSourceRemoved(i);
      --i;
    }
  }
  // Iterate through the new sources to find the added sources.
  if (new_sources.size() > sources_.size()) {
    SourceSet old_source_set;
    for (size_t i = 0; i < sources_.size(); ++i) {
      old_source_set.insert(sources_[i].id);
    }

    for (size_t i = 0; i < new_sources.size(); ++i) {
      if (old_source_set.find(new_sources[i].id) == old_source_set.end()) {
        sources_.insert(sources_.begin() + i, Source());
        sources_[i].id = new_sources[i].id;
        sources_[i].name = new_sources[i].name;
        observer_->OnSourceAdded(i);
      }
    }
  }
  DCHECK_EQ(new_sources.size(), sources_.size());

  // Find the moved/changed sources.
  size_t pos = 0;
  while (pos < sources_.size()) {
    if (!(sources_[pos].id == new_sources[pos].id)) {
      // Find the source that should be moved to |pos|, starting from |pos + 1|
      // of |sources_|, because entries before |pos| should have been sorted.
      size_t old_pos = pos + 1;
      for (; old_pos < sources_.size(); ++old_pos) {
        if (sources_[old_pos].id == new_sources[pos].id)
          break;
      }
      DCHECK(sources_[old_pos].id == new_sources[pos].id);

      // Move the source from |old_pos| to |pos|.
      Source temp = sources_[old_pos];
      sources_.erase(sources_.begin() + old_pos);
      sources_.insert(sources_.begin() + pos, temp);

      observer_->OnSourceMoved(old_pos, pos);
    }

    if (sources_[pos].name != new_sources[pos].name) {
      sources_[pos].name = new_sources[pos].name;
      observer_->OnSourceNameChanged(pos);
    }
    ++pos;
  }
}

void DesktopMediaListAsh::EnumerateWindowsForRoot(
    std::vector<DesktopMediaListAsh::SourceDescription>* sources,
    aura::Window* root_window,
    int container_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  aura::Window* container = ash::Shell::GetContainer(root_window, container_id);
  if (!container)
    return;
  for (aura::Window::Windows::const_iterator it = container->children().begin();
       it != container->children().end(); ++it) {
    if (!(*it)->IsVisible() || !(*it)->CanFocus())
      continue;
    content::DesktopMediaID id =
        content::DesktopMediaID::RegisterAuraWindow(*it);
    if (id.id == view_dialog_id_)
      continue;
    SourceDescription window_source(id, (*it)->title());
    sources->push_back(window_source);

    CaptureThumbnail(window_source.id, *it);
  }
}

void DesktopMediaListAsh::EnumerateSources(
    std::vector<DesktopMediaListAsh::SourceDescription>* sources) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  for (size_t i = 0; i < root_windows.size(); ++i) {
    if (source_types_ & SCREENS) {
      SourceDescription screen_source(
          content::DesktopMediaID::RegisterAuraWindow(root_windows[i]),
          root_windows[i]->title());

      if (root_windows[i] == ash::Shell::GetPrimaryRootWindow())
        sources->insert(sources->begin(), screen_source);
      else
        sources->push_back(screen_source);

      if (screen_source.name.empty()) {
        if (root_windows.size() > 1) {
          screen_source.name = l10n_util::GetStringFUTF16Int(
              IDS_DESKTOP_MEDIA_PICKER_MULTIPLE_SCREEN_NAME,
              static_cast<int>(i + 1));
        } else {
          screen_source.name = l10n_util::GetStringUTF16(
              IDS_DESKTOP_MEDIA_PICKER_SINGLE_SCREEN_NAME);
        }
      }

      CaptureThumbnail(screen_source.id, root_windows[i]);
    }

    if (source_types_ & WINDOWS) {
      EnumerateWindowsForRoot(
          sources, root_windows[i], ash::kShellWindowId_DefaultContainer);
      EnumerateWindowsForRoot(
          sources, root_windows[i], ash::kShellWindowId_AlwaysOnTopContainer);
      EnumerateWindowsForRoot(
          sources, root_windows[i], ash::kShellWindowId_DockedContainer);
    }
  }
}

void DesktopMediaListAsh::CaptureThumbnail(content::DesktopMediaID id,
                                           aura::Window* window) {
  gfx::Rect window_rect(window->bounds().width(), window->bounds().height());
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(thumbnail_size_), window_rect.size());

  ++pending_window_capture_requests_;
  ui::GrabWindowSnapshotAndScaleAsync(
      window,
      window_rect,
      scaled_rect.size(),
      BrowserThread::GetBlockingPool(),
      base::Bind(&DesktopMediaListAsh::OnThumbnailCaptured,
                 weak_factory_.GetWeakPtr(),
                 id));
}

void DesktopMediaListAsh::OnThumbnailCaptured(content::DesktopMediaID id,
                                              const gfx::Image& image) {
  for (size_t i = 0; i < sources_.size(); ++i) {
    if (sources_[i].id == id) {
      sources_[i].thumbnail = image.AsImageSkia();
      observer_->OnSourceThumbnailChanged(i);
      break;
    }
  }

  --pending_window_capture_requests_;
  DCHECK_GE(pending_window_capture_requests_, 0);

  if (!pending_window_capture_requests_) {
    // Once we've finished capturing all windows post a task for the next list
    // update.
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DesktopMediaListAsh::Refresh,
                   weak_factory_.GetWeakPtr()),
        update_period_);
  }
}
