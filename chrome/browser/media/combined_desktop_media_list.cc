// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/combined_desktop_media_list.h"

CombinedDesktopMediaList::CombinedDesktopMediaList(
    std::vector<scoped_ptr<DesktopMediaList>>& media_lists)
    : media_lists_(std::move(media_lists)) {
  DCHECK(media_lists_.size());
}

CombinedDesktopMediaList::~CombinedDesktopMediaList() {}

void CombinedDesktopMediaList::SetUpdatePeriod(base::TimeDelta period) {
  for (const auto& list : media_lists_)
    list->SetUpdatePeriod(period);
}

void CombinedDesktopMediaList::SetThumbnailSize(
    const gfx::Size& thumbnail_size) {
  for (const auto& list : media_lists_)
    list->SetThumbnailSize(thumbnail_size);
}

void CombinedDesktopMediaList::SetViewDialogWindowId(
    content::DesktopMediaID dialog_id) {
  for (const auto& list : media_lists_)
    list->SetViewDialogWindowId(dialog_id);
}

void CombinedDesktopMediaList::StartUpdating(
    DesktopMediaListObserver* observer) {
  observer_ = observer;
  for (const auto& list : media_lists_)
    list->StartUpdating(this);
}

int CombinedDesktopMediaList::GetSourceCount() const {
  int count = 0;
  for (const auto& list : media_lists_)
    count += list->GetSourceCount();

  return count;
}

const DesktopMediaList::Source& CombinedDesktopMediaList::GetSource(
    int index) const {
  for (const auto& list : media_lists_) {
    int count = list->GetSourceCount();
    if (index < count)
      return list->GetSource(index);

    index -= count;
  }

  NOTREACHED();
  return media_lists_[0]->GetSource(0);
}

void CombinedDesktopMediaList::OnSourceAdded(DesktopMediaList* list,
                                             int index) {
  int count = 0;
  for (const auto& cur_list : media_lists_) {
    if (cur_list.get() == list) {
      observer_->OnSourceAdded(this, count + index);
      return;
    }
    count += cur_list->GetSourceCount();
  }

  NOTREACHED();
}

void CombinedDesktopMediaList::OnSourceRemoved(DesktopMediaList* list,
                                               int index) {
  int count = 0;
  for (const auto& cur_list : media_lists_) {
    if (cur_list.get() == list) {
      observer_->OnSourceRemoved(this, count + index);
      return;
    }
    count += cur_list->GetSourceCount();
  }

  NOTREACHED();
}

void CombinedDesktopMediaList::OnSourceMoved(DesktopMediaList* list,
                                             int old_index,
                                             int new_index) {
  int count = 0;
  for (const auto& cur_list : media_lists_) {
    if (cur_list.get() == list) {
      observer_->OnSourceMoved(this, count + old_index, count + new_index);
      return;
    }
    count += cur_list->GetSourceCount();
  }

  NOTREACHED();
}

void CombinedDesktopMediaList::OnSourceNameChanged(DesktopMediaList* list,
                                                   int index) {
  int count = 0;
  for (const auto& cur_list : media_lists_) {
    if (cur_list.get() == list) {
      observer_->OnSourceNameChanged(this, count + index);
      return;
    }
    count += cur_list->GetSourceCount();
  }

  NOTREACHED();
}

void CombinedDesktopMediaList::OnSourceThumbnailChanged(DesktopMediaList* list,
                                                        int index) {
  int count = 0;
  for (const auto& cur_list : media_lists_) {
    if (cur_list.get() == list) {
      observer_->OnSourceThumbnailChanged(this, count + index);
      return;
    }
    count += cur_list->GetSourceCount();
  }

  NOTREACHED();
}
