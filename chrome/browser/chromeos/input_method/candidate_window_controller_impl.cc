// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/candidate_window_controller_impl.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/candidate_window_view.h"
#include "chrome/browser/chromeos/input_method/infolist_window_view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#endif  // USE_ASH

namespace chromeos {
namespace input_method {

namespace {
// The milliseconds of the delay to show the infolist window.
const int kInfolistShowDelayMilliSeconds = 500;
// The milliseconds of the delay to hide the infolist window.
const int kInfolistHideDelayMilliSeconds = 500;
}  // namespace

bool CandidateWindowControllerImpl::Init() {
  // Create the candidate window view.
  CreateView();

  // The observer should be added before Connect() so we can capture the
  // initial connection change.
  ibus_ui_controller_->AddObserver(this);
  ibus_ui_controller_->Connect();
  return true;
}

void CandidateWindowControllerImpl::CreateView() {
  // Create a non-decorated frame.
  frame_.reset(new views::Widget);
  // The size is initially zero.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  // |frame_| and |infolist_frame_| are owned by controller impl so
  // they should use WIDGET_OWNS_NATIVE_WIDGET ownership.
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Show the candidate window always on top
#if defined(USE_ASH)
  params.parent = ash::Shell::GetContainer(
      ash::Shell::GetActiveRootWindow(),
      ash::internal::kShellWindowId_InputMethodContainer);
#else
  params.keep_on_top = true;
#endif
  frame_->Init(params);
#if defined(USE_ASH)
  ash::SetWindowVisibilityAnimationType(
      frame_->GetNativeView(),
      ash::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
#endif  // USE_ASH

  // Create the candidate window.
  candidate_window_ = new CandidateWindowView(frame_.get());
  candidate_window_->Init();
  candidate_window_->AddObserver(this);

  frame_->SetContentsView(candidate_window_);


  // Create the infolist window.
  infolist_frame_.reset(new views::Widget);
  infolist_frame_->Init(params);
#if defined(USE_ASH)
  ash::SetWindowVisibilityAnimationType(
      infolist_frame_->GetNativeView(),
      ash::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
#endif  // USE_ASH

  infolist_window_ = new InfolistWindowView(
      infolist_frame_.get(), frame_.get());
  infolist_window_->Init();
  infolist_frame_->SetContentsView(infolist_window_);
}

CandidateWindowControllerImpl::CandidateWindowControllerImpl()
    : ibus_ui_controller_(IBusUiController::Create()),
      candidate_window_(NULL),
      infolist_window_(NULL) {
}

CandidateWindowControllerImpl::~CandidateWindowControllerImpl() {
  ibus_ui_controller_->RemoveObserver(this);
  candidate_window_->RemoveObserver(this);
  // ibus_ui_controller_'s destructor will close the connection.
}

void CandidateWindowControllerImpl::OnHideAuxiliaryText() {
  candidate_window_->HideAuxiliaryText();
}

void CandidateWindowControllerImpl::OnHideLookupTable() {
  candidate_window_->HideLookupTable();
  infolist_window_->Hide();
}

void CandidateWindowControllerImpl::OnHidePreeditText() {
  candidate_window_->HidePreeditText();
}

void CandidateWindowControllerImpl::OnSetCursorLocation(
    const gfx::Rect& cursor_location,
    const gfx::Rect& composition_head) {
  // A workaround for http://crosbug.com/6460. We should ignore very short Y
  // move to prevent the window from shaking up and down.
  const int kKeepPositionThreshold = 2;  // px
  const gfx::Rect& last_location =
      candidate_window_->cursor_location();
  const int delta_y = abs(last_location.y() - cursor_location.y());
  if ((last_location.x() == cursor_location.x()) &&
      (delta_y <= kKeepPositionThreshold)) {
    DVLOG(1) << "Ignored set_cursor_location signal to prevent window shake";
    return;
  }

  // Remember the cursor location.
  candidate_window_->set_cursor_location(cursor_location);
  candidate_window_->set_composition_head_location(composition_head);
  // Move the window per the cursor location.
  candidate_window_->ResizeAndMoveParentFrame();
  infolist_window_->ResizeAndMoveParentFrame();
}

void CandidateWindowControllerImpl::OnUpdateAuxiliaryText(
    const std::string& utf8_text,
    bool visible) {
  // If it's not visible, hide the auxiliary text and return.
  if (!visible) {
    candidate_window_->HideAuxiliaryText();
    return;
  }
  candidate_window_->UpdateAuxiliaryText(utf8_text);
  candidate_window_->ShowAuxiliaryText();
}

void CandidateWindowControllerImpl::OnUpdateLookupTable(
    const InputMethodLookupTable& lookup_table) {
  // If it's not visible, hide the lookup table and return.
  if (!lookup_table.visible) {
    candidate_window_->HideLookupTable();
    infolist_window_->Hide();
    return;
  }

  candidate_window_->UpdateCandidates(lookup_table);
  candidate_window_->ShowLookupTable();

  // TODO(nona): Remove mozc::commands dependencies.
  const mozc::commands::Candidates& candidates = lookup_table.mozc_candidates;

  std::vector<InfolistWindowView::Entry> infolist_entries;
  const mozc::commands::InformationList& usages = candidates.usages();

  for (int i = 0; i < usages.information_size(); ++i) {
    InfolistWindowView::Entry entry;
    entry.title = usages.information(i).title();
    entry.body = usages.information(i).description();
    infolist_entries.push_back(entry);
  }

  // If there is no infolist entry, just hide.
  if (infolist_entries.empty()) {
    infolist_window_->Hide();
    return;
  }

  // TODO(nona): Return if there is no change in infolist entries after complete
  // mozc dependency removal.

  int focused_index = infolist_entries.size();
  if (usages.has_focused_index())
    focused_index = usages.focused_index();

  infolist_window_->UpdateCandidates(infolist_entries, focused_index);
  if (candidates.has_focused_index() && candidates.candidate_size() > 0) {
    const int focused_row =
        candidates.focused_index() - candidates.candidate(0).index();
    if (candidates.candidate_size() >= focused_row &&
        candidates.candidate(focused_row).has_information_id()) {
      infolist_window_->DelayShow(kInfolistShowDelayMilliSeconds);
    } else {
      infolist_window_->DelayHide(kInfolistHideDelayMilliSeconds);
    }
  } else {
    infolist_window_->DelayHide(kInfolistHideDelayMilliSeconds);
  }
}

void CandidateWindowControllerImpl::OnUpdatePreeditText(
    const std::string& utf8_text, unsigned int cursor, bool visible) {
  // If it's not visible, hide the preedit text and return.
  if (!visible || utf8_text.empty()) {
    candidate_window_->HidePreeditText();
    return;
  }
  candidate_window_->UpdatePreeditText(utf8_text);
  candidate_window_->ShowPreeditText();
}

void CandidateWindowControllerImpl::OnCandidateCommitted(int index,
                                                         int button,
                                                         int flags) {
  ibus_ui_controller_->NotifyCandidateClicked(index, button, flags);
}

void CandidateWindowControllerImpl::OnCandidateWindowOpened() {
  FOR_EACH_OBSERVER(CandidateWindowController::Observer, observers_,
                    CandidateWindowOpened());
}

void CandidateWindowControllerImpl::OnCandidateWindowClosed() {
  FOR_EACH_OBSERVER(CandidateWindowController::Observer, observers_,
                    CandidateWindowClosed());
}

void CandidateWindowControllerImpl::AddObserver(
    CandidateWindowController::Observer* observer) {
  observers_.AddObserver(observer);
}

void CandidateWindowControllerImpl::RemoveObserver(
    CandidateWindowController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CandidateWindowControllerImpl::OnConnectionChange(bool connected) {
  if (!connected) {
    candidate_window_->HideAll();
    infolist_window_->Hide();
  }
}

}  // namespace input_method
}  // namespace chromeos
