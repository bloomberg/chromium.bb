// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_COMBINED_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_COMBINED_DESKTOP_MEDIA_LIST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media/desktop_media_list.h"
#include "chrome/browser/media/desktop_media_list_observer.h"

// The combined desktop media list of one or multiple input DesktopMediaLists.
// The combined list is the concatenation of the input lists. The front of
// combined list will be filled with the first input list, then follows with
// the second one and so on.
class CombinedDesktopMediaList : public DesktopMediaList,
                                 public DesktopMediaListObserver {
 public:
  explicit CombinedDesktopMediaList(
      std::vector<scoped_ptr<DesktopMediaList>>& media_lists);
  ~CombinedDesktopMediaList() override;

  // DesktopMediaList interface.
  void SetUpdatePeriod(base::TimeDelta period) override;
  void SetThumbnailSize(const gfx::Size& thumbnail_size) override;
  void SetViewDialogWindowId(content::DesktopMediaID dialog_id) override;
  void StartUpdating(DesktopMediaListObserver* observer) override;
  int GetSourceCount() const override;
  const Source& GetSource(int index) const override;

 private:
  // DesktopMediaListObserver interface.
  void OnSourceAdded(DesktopMediaList* list, int index) override;
  void OnSourceRemoved(DesktopMediaList* list, int index) override;
  void OnSourceMoved(DesktopMediaList* list,
                     int old_index,
                     int new_index) override;
  void OnSourceNameChanged(DesktopMediaList* list, int index) override;
  void OnSourceThumbnailChanged(DesktopMediaList* list, int index) override;

  std::vector<scoped_ptr<DesktopMediaList>> media_lists_;

  DesktopMediaListObserver* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CombinedDesktopMediaList);
};

#endif  // CHROME_BROWSER_MEDIA_COMBINED_DESKTOP_MEDIA_LIST_H_
