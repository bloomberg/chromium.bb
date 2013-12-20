// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/candidate_window_controller_impl.h"

#include <string>
#include <vector>

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/candidate_window_view.h"
#include "chrome/browser/chromeos/input_method/infolist_window.h"
#include "chrome/browser/chromeos/input_method/mode_indicator_controller.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"


namespace chromeos {
namespace input_method {

namespace {

}  // namespace

CandidateWindowControllerImpl::CandidateWindowControllerImpl()
    : candidate_window_view_(NULL),
      infolist_window_(NULL) {
  IBusBridge::Get()->SetCandidateWindowHandler(this);
  CreateView();
}

CandidateWindowControllerImpl::~CandidateWindowControllerImpl() {
  IBusBridge::Get()->SetCandidateWindowHandler(NULL);
  candidate_window_view_->RemoveObserver(this);
}

void CandidateWindowControllerImpl::CreateView() {
  // Create a non-decorated frame.
  frame_.reset(new views::Widget);
  // The size is initially zero.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  // |frame_| is owned by controller impl so
  // they should use WIDGET_OWNS_NATIVE_WIDGET ownership.
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Show the candidate window always on top
  params.parent = ash::Shell::GetContainer(
      ash::Shell::GetTargetRootWindow(),
      ash::internal::kShellWindowId_InputMethodContainer);
  frame_->Init(params);

  views::corewm::SetWindowVisibilityAnimationType(
      frame_->GetNativeView(),
      views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);

  // Create the candidate window.
  candidate_window_view_ = new CandidateWindowView(frame_.get());
  candidate_window_view_->Init();
  candidate_window_view_->AddObserver(this);

  frame_->SetContentsView(candidate_window_view_);

  // Create the mode indicator controller.
  mode_indicator_controller_.reset(
      new ModeIndicatorController(InputMethodManager::Get()));
}

void CandidateWindowControllerImpl::Hide() {
  // To hide the candidate window we have to call HideLookupTable and
  // HideAuxiliaryText. Without calling HideAuxiliaryText the
  // auxiliary text area will remain.
  candidate_window_view_->HideLookupTable();
  candidate_window_view_->HideAuxiliaryText();
  if (infolist_window_)
    infolist_window_->HideImmediately();
}

void CandidateWindowControllerImpl::SetCursorBounds(
    const gfx::Rect& cursor_bounds,
    const gfx::Rect& composition_head) {
  // A workaround for http://crosbug.com/6460. We should ignore very short Y
  // move to prevent the window from shaking up and down.
  const int kKeepPositionThreshold = 2;  // px
  const gfx::Rect& last_bounds =
      candidate_window_view_->cursor_bounds();
  const int delta_y = abs(last_bounds.y() - cursor_bounds.y());
  if ((last_bounds.x() == cursor_bounds.x()) &&
      (delta_y <= kKeepPositionThreshold)) {
    DVLOG(1) << "Ignored set_cursor_bounds signal to prevent window shake";
    return;
  }

  // Remember the cursor bounds.
  candidate_window_view_->set_cursor_bounds(cursor_bounds);
  candidate_window_view_->set_composition_head_bounds(composition_head);
  // Move the window per the cursor bounds.
  candidate_window_view_->ResizeAndMoveParentFrame();
  if (infolist_window_)
    infolist_window_->GetWidget()->SetBounds(GetInfolistBounds());

  // Mode indicator controller also needs the cursor bounds.
  mode_indicator_controller_->SetCursorBounds(cursor_bounds);
}

void CandidateWindowControllerImpl::UpdateAuxiliaryText(
    const std::string& utf8_text,
    bool visible) {
  // If it's not visible, hide the auxiliary text and return.
  if (!visible) {
    candidate_window_view_->HideAuxiliaryText();
    return;
  }
  candidate_window_view_->UpdateAuxiliaryText(utf8_text);
  candidate_window_view_->ShowAuxiliaryText();
}

void CandidateWindowControllerImpl::FocusStateChanged(bool is_focused) {
  mode_indicator_controller_->FocusStateChanged(is_focused);
}

// static
void CandidateWindowControllerImpl::ConvertLookupTableToInfolistEntry(
    const CandidateWindow& candidate_window,
    std::vector<InfolistEntry>* infolist_entries,
    bool* has_highlighted) {
  DCHECK(infolist_entries);
  DCHECK(has_highlighted);
  infolist_entries->clear();
  *has_highlighted = false;

  const size_t cursor_index_in_page =
      candidate_window.cursor_position() % candidate_window.page_size();

  for (size_t i = 0; i < candidate_window.candidates().size(); ++i) {
    const CandidateWindow::Entry& ibus_entry =
        candidate_window.candidates()[i];
    if (ibus_entry.description_title.empty() &&
        ibus_entry.description_body.empty())
      continue;
    InfolistEntry entry(UTF8ToUTF16(ibus_entry.description_title),
                        UTF8ToUTF16(ibus_entry.description_body));
    if (i == cursor_index_in_page) {
      entry.highlighted = true;
      *has_highlighted = true;
    }
    infolist_entries->push_back(entry);
  }
}

void CandidateWindowControllerImpl::UpdateLookupTable(
    const CandidateWindow& candidate_window,
    bool visible) {
  // If it's not visible, hide the lookup table and return.
  if (!visible) {
    candidate_window_view_->HideLookupTable();
    if (infolist_window_)
      infolist_window_->HideImmediately();
    // TODO(nona): Introduce unittests for crbug.com/170036.
    latest_infolist_entries_.clear();
    return;
  }

  candidate_window_view_->UpdateCandidates(candidate_window);
  candidate_window_view_->ShowLookupTable();

  bool has_highlighted = false;
  std::vector<InfolistEntry> infolist_entries;
  ConvertLookupTableToInfolistEntry(candidate_window, &infolist_entries,
                                    &has_highlighted);

  // If there is no change, just return.
  if (latest_infolist_entries_ == infolist_entries)
    return;

  latest_infolist_entries_ = infolist_entries;

  if (infolist_entries.empty()) {
    if (infolist_window_)
      infolist_window_->HideImmediately();
    return;
  }

  // Highlight moves out of the infolist entries.
  if (!has_highlighted) {
    if (infolist_window_)
      infolist_window_->HideWithDelay();
    return;
  }

  if (infolist_window_) {
    infolist_window_->Relayout(infolist_entries);
    infolist_window_->GetWidget()->SetBounds(GetInfolistBounds());
  } else {
    infolist_window_ = new InfolistWindow(infolist_entries);
    infolist_window_->InitWidget(
        ash::Shell::GetContainer(
            ash::Shell::GetTargetRootWindow(),
            ash::internal::kShellWindowId_InputMethodContainer),
        GetInfolistBounds());
    infolist_window_->GetWidget()->AddObserver(this);
  }
  infolist_window_->ShowWithDelay();
}

gfx::Rect CandidateWindowControllerImpl::GetInfolistBounds() {
  gfx::Rect new_bounds(infolist_window_->GetPreferredSize());
  // Infolist has to be in the same display of the candidate window.
  gfx::NativeWindow native_frame = frame_->GetNativeWindow();
  new_bounds.set_origin(GetInfolistWindowPosition(
        frame_->GetClientAreaBoundsInScreen(),
        gfx::Screen::GetScreenFor(native_frame)->GetDisplayNearestWindow(
            native_frame).work_area(),
        new_bounds.size()));
  return new_bounds;
}

void CandidateWindowControllerImpl::UpdatePreeditText(
    const std::string& utf8_text, unsigned int cursor, bool visible) {
  // If it's not visible, hide the preedit text and return.
  if (!visible || utf8_text.empty()) {
    candidate_window_view_->HidePreeditText();
    return;
  }
  candidate_window_view_->UpdatePreeditText(utf8_text);
  candidate_window_view_->ShowPreeditText();
}

void CandidateWindowControllerImpl::OnCandidateCommitted(int index) {
  FOR_EACH_OBSERVER(CandidateWindowController::Observer, observers_,
                    CandidateClicked(index));
}

void CandidateWindowControllerImpl::OnCandidateWindowOpened() {
  FOR_EACH_OBSERVER(CandidateWindowController::Observer, observers_,
                    CandidateWindowOpened());
}

void CandidateWindowControllerImpl::OnCandidateWindowClosed() {
  FOR_EACH_OBSERVER(CandidateWindowController::Observer, observers_,
                    CandidateWindowClosed());
}

void CandidateWindowControllerImpl::OnWidgetClosing(views::Widget* widget) {
  if (infolist_window_ && widget == infolist_window_->GetWidget()) {
    widget->RemoveObserver(this);
    infolist_window_ = NULL;
  }
}

void CandidateWindowControllerImpl::AddObserver(
    CandidateWindowController::Observer* observer) {
  observers_.AddObserver(observer);
}

void CandidateWindowControllerImpl::RemoveObserver(
    CandidateWindowController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
gfx::Point CandidateWindowControllerImpl::GetInfolistWindowPosition(
    const gfx::Rect& candidate_window_view_rect,
    const gfx::Rect& screen_rect,
    const gfx::Size& infolist_window_size) {
  gfx::Point result(candidate_window_view_rect.right(),
                    candidate_window_view_rect.y());

  if (candidate_window_view_rect.right() + infolist_window_size.width() >
      screen_rect.right())
    result.set_x(candidate_window_view_rect.x() - infolist_window_size.width());

  if (candidate_window_view_rect.y() + infolist_window_size.height() >
      screen_rect.bottom())
    result.set_y(screen_rect.bottom() - infolist_window_size.height());

  return result;
}

}  // namespace input_method
}  // namespace chromeos
