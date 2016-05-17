// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/focus/focus_manager.h"

using content::DesktopMediaID;

namespace {

const int kDesktopMediaSourceViewGroupId = 1;

}  // namespace

DesktopMediaListView::DesktopMediaListView(
    DesktopMediaPickerDialogView* parent,
    std::unique_ptr<DesktopMediaList> media_list)
    : parent_(parent), media_list_(std::move(media_list)), weak_factory_(this) {
  media_list_->SetThumbnailSize(
      gfx::Size(DesktopMediaPickerDialogView::kThumbnailWidth,
                DesktopMediaPickerDialogView::kThumbnailHeight));
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

DesktopMediaListView::~DesktopMediaListView() {}

void DesktopMediaListView::StartUpdating(DesktopMediaID dialog_window_id) {
  media_list_->SetViewDialogWindowId(dialog_window_id);
  media_list_->StartUpdating(this);
}

void DesktopMediaListView::OnSelectionChanged() {
  parent_->OnSelectionChanged();
}

void DesktopMediaListView::OnDoubleClick() {
  parent_->OnDoubleClick();
}

DesktopMediaSourceView* DesktopMediaListView::GetSelection() {
  for (int i = 0; i < child_count(); ++i) {
    DesktopMediaSourceView* source_view =
        static_cast<DesktopMediaSourceView*>(child_at(i));
    DCHECK_EQ(source_view->GetClassName(),
              DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
    if (source_view->is_selected())
      return source_view;
  }
  return NULL;
}

gfx::Size DesktopMediaListView::GetPreferredSize() const {
  int total_rows =
      (child_count() + DesktopMediaPickerDialogView::kListColumns - 1) /
      DesktopMediaPickerDialogView::kListColumns;
  return gfx::Size(DesktopMediaPickerDialogView::kTotalListWidth,
                   DesktopMediaPickerDialogView::kListItemHeight * total_rows);
}

void DesktopMediaListView::Layout() {
  int x = 0;
  int y = 0;

  for (int i = 0; i < child_count(); ++i) {
    if (x + DesktopMediaPickerDialogView::kListItemWidth >
        DesktopMediaPickerDialogView::kTotalListWidth) {
      x = 0;
      y += DesktopMediaPickerDialogView::kListItemHeight;
    }

    View* source_view = child_at(i);
    source_view->SetBounds(x, y, DesktopMediaPickerDialogView::kListItemWidth,
                           DesktopMediaPickerDialogView::kListItemHeight);

    x += DesktopMediaPickerDialogView::kListItemWidth;
  }

  y += DesktopMediaPickerDialogView::kListItemHeight;
}

bool DesktopMediaListView::OnKeyPressed(const ui::KeyEvent& event) {
  int position_increment = 0;
  switch (event.key_code()) {
    case ui::VKEY_UP:
      position_increment = -DesktopMediaPickerDialogView::kListColumns;
      break;
    case ui::VKEY_DOWN:
      position_increment = DesktopMediaPickerDialogView::kListColumns;
      break;
    case ui::VKEY_LEFT:
      position_increment = -1;
      break;
    case ui::VKEY_RIGHT:
      position_increment = 1;
      break;
    default:
      return false;
  }

  if (position_increment != 0) {
    DesktopMediaSourceView* selected = GetSelection();
    DesktopMediaSourceView* new_selected = NULL;

    if (selected) {
      int index = GetIndexOf(selected);
      int new_index = index + position_increment;
      if (new_index >= child_count())
        new_index = child_count() - 1;
      else if (new_index < 0)
        new_index = 0;
      if (index != new_index) {
        new_selected =
            static_cast<DesktopMediaSourceView*>(child_at(new_index));
      }
    } else if (has_children()) {
      new_selected = static_cast<DesktopMediaSourceView*>(child_at(0));
    }

    if (new_selected) {
      GetFocusManager()->SetFocusedView(new_selected);
    }

    return true;
  }

  return false;
}

void DesktopMediaListView::OnSourceAdded(DesktopMediaList* list, int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  DesktopMediaSourceView* source_view =
      new DesktopMediaSourceView(this, source.id);
  source_view->SetName(source.name);
  source_view->SetGroup(kDesktopMediaSourceViewGroupId);
  AddChildViewAt(source_view, index);

  PreferredSizeChanged();

  if (child_count() % DesktopMediaPickerDialogView::kListColumns == 1)
    parent_->OnMediaListRowsChanged();

  // Auto select the first screen.
  if (index == 0 && source.id.type == DesktopMediaID::TYPE_SCREEN)
    source_view->RequestFocus();

  std::string autoselect_source =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutoSelectDesktopCaptureSource);
  if (!autoselect_source.empty() &&
      base::ASCIIToUTF16(autoselect_source) == source.name) {
    // Select, then accept and close the dialog when we're done adding sources.
    source_view->OnFocus();
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DesktopMediaListView::AcceptSelection,
                   weak_factory_.GetWeakPtr()));
  }
}

void DesktopMediaListView::OnSourceRemoved(DesktopMediaList* list, int index) {
  DesktopMediaSourceView* view =
      static_cast<DesktopMediaSourceView*>(child_at(index));
  DCHECK(view);
  DCHECK_EQ(view->GetClassName(),
            DesktopMediaSourceView::kDesktopMediaSourceViewClassName);
  bool was_selected = view->is_selected();
  RemoveChildView(view);
  delete view;

  if (was_selected)
    OnSelectionChanged();

  PreferredSizeChanged();

  if (child_count() % DesktopMediaPickerDialogView::kListColumns == 0)
    parent_->OnMediaListRowsChanged();
}

void DesktopMediaListView::OnSourceMoved(DesktopMediaList* list,
                                         int old_index,
                                         int new_index) {
  DesktopMediaSourceView* view =
      static_cast<DesktopMediaSourceView*>(child_at(old_index));
  ReorderChildView(view, new_index);
  PreferredSizeChanged();
}

void DesktopMediaListView::OnSourceNameChanged(DesktopMediaList* list,
                                               int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  DesktopMediaSourceView* source_view =
      static_cast<DesktopMediaSourceView*>(child_at(index));
  source_view->SetName(source.name);
}

void DesktopMediaListView::OnSourceThumbnailChanged(DesktopMediaList* list,
                                                    int index) {
  const DesktopMediaList::Source& source = media_list_->GetSource(index);
  DesktopMediaSourceView* source_view =
      static_cast<DesktopMediaSourceView*>(child_at(index));
  source_view->SetThumbnail(source.thumbnail);
}

void DesktopMediaListView::AcceptSelection() {
  OnSelectionChanged();
  OnDoubleClick();
}
