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
    std::unique_ptr<DesktopMediaList> media_list,
    DesktopMediaSourceViewStyle generic_style,
    DesktopMediaSourceViewStyle single_style)
    : parent_(parent),
      media_list_(std::move(media_list)),
      single_style_(single_style),
      generic_style_(generic_style),
      active_style_(&single_style_),
      weak_factory_(this) {
  SetStyle(&single_style_);

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
  return nullptr;
}

gfx::Size DesktopMediaListView::GetPreferredSize() const {
  int total_rows =
      (child_count() + active_style_->columns - 1) / active_style_->columns;
  return gfx::Size(active_style_->columns * active_style_->item_size.width(),
                   total_rows * active_style_->item_size.height());
}

void DesktopMediaListView::Layout() {
  int x = 0;
  int y = 0;

  for (int i = 0; i < child_count(); ++i) {
    if (i > 0 && i % active_style_->columns == 0) {
      x = 0;
      y += active_style_->item_size.height();
    }

    child_at(i)->SetBounds(x, y, active_style_->item_size.width(),
                           active_style_->item_size.height());

    x += active_style_->item_size.width();
  }
}

bool DesktopMediaListView::OnKeyPressed(const ui::KeyEvent& event) {
  int position_increment = 0;
  switch (event.key_code()) {
    case ui::VKEY_UP:
      position_increment = -active_style_->columns;
      break;
    case ui::VKEY_DOWN:
      position_increment = active_style_->columns;
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
    DesktopMediaSourceView* new_selected = nullptr;

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

  // We are going to have a second item, apply the generic style.
  if (child_count() == 1)
    SetStyle(&generic_style_);

  DesktopMediaSourceView* source_view =
      new DesktopMediaSourceView(this, source.id, *active_style_);

  source_view->SetName(source.name);
  source_view->SetGroup(kDesktopMediaSourceViewGroupId);
  AddChildViewAt(source_view, index);

  if ((child_count() - 1) % active_style_->columns == 0)
    parent_->OnMediaListRowsChanged();

  // Auto select the first screen.
  if (index == 0 && source.id.type == DesktopMediaID::TYPE_SCREEN)
    source_view->RequestFocus();

  PreferredSizeChanged();

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

  if (child_count() % active_style_->columns == 0)
    parent_->OnMediaListRowsChanged();

  // Apply single-item styling when the second source is removed.
  if (child_count() == 1)
    SetStyle(&single_style_);

  PreferredSizeChanged();
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

void DesktopMediaListView::SetStyle(DesktopMediaSourceViewStyle* style) {
  active_style_ = style;
  media_list_->SetThumbnailSize(gfx::Size(
      style->image_rect.width() - 2 * style->selection_border_thickness,
      style->image_rect.height() - 2 * style->selection_border_thickness));

  for (int i = 0; i < child_count(); i++) {
    DesktopMediaSourceView* source_view =
        static_cast<DesktopMediaSourceView*>(child_at(i));
    source_view->SetStyle(*active_style_);
  }
}
