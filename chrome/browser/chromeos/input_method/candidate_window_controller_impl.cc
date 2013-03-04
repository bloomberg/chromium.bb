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
#include "chrome/browser/chromeos/input_method/delayable_widget.h"
#include "chrome/browser/chromeos/input_method/infolist_window_view.h"
#include "chromeos/dbus/dbus_thread_manager.h"
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

// Converts from ibus::Rect to gfx::Rect.
gfx::Rect IBusRectToGfxRect(const ibus::Rect& rect) {
  return gfx::Rect(rect.x, rect.y, rect.width, rect.height);
}

// Returns pointer of IBusPanelService. This function returns NULL if it is not
// ready.
IBusPanelService* GetIBusPanelService() {
  return DBusThreadManager::Get()->GetIBusPanelService();
}
}  // namespace

bool CandidateWindowControllerImpl::Init(IBusController* controller) {
  IBusDaemonController::GetInstance()->AddObserver(this);
  // Create the candidate window view.
  CreateView();
  return true;
}

void CandidateWindowControllerImpl::Shutdown(IBusController* controller) {
  IBusDaemonController::GetInstance()->RemoveObserver(this);
}

void CandidateWindowControllerImpl::CreateView() {
  // Create a non-decorated frame.
  frame_.reset(new views::Widget);
  // The size is initially zero.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  // |frame_| and |infolist_window_| are owned by controller impl so
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
  views::corewm::SetWindowVisibilityAnimationType(
      frame_->GetNativeView(),
      views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
#endif  // USE_ASH

  // Create the candidate window.
  candidate_window_ = new CandidateWindowView(frame_.get());
  candidate_window_->Init();
  candidate_window_->AddObserver(this);

  frame_->SetContentsView(candidate_window_);


  // Create the infolist window.
  infolist_window_.reset(new DelayableWidget);
  infolist_window_->Init(params);
#if defined(USE_ASH)
  views::corewm::SetWindowVisibilityAnimationType(
      infolist_window_->GetNativeView(),
      views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
#endif  // USE_ASH

  InfolistWindowView* infolist_view = new InfolistWindowView;
  infolist_view->Init();
  infolist_window_->SetContentsView(infolist_view);
}

CandidateWindowControllerImpl::CandidateWindowControllerImpl()
    : candidate_window_(NULL),
      infolist_window_(NULL),
      latest_infolist_focused_index_(InfolistWindowView::InvalidFocusIndex()) {
}

CandidateWindowControllerImpl::~CandidateWindowControllerImpl() {
  if (DBusThreadManager::Get()->GetIBusPanelService())
    DBusThreadManager::Get()->GetIBusPanelService()->
        SetUpCandidateWindowHandler(NULL);
  candidate_window_->RemoveObserver(this);
}

void CandidateWindowControllerImpl::HideAuxiliaryText() {
  candidate_window_->HideAuxiliaryText();
}

void CandidateWindowControllerImpl::HideLookupTable() {
  candidate_window_->HideLookupTable();
  infolist_window_->Hide();
}

void CandidateWindowControllerImpl::HidePreeditText() {
  candidate_window_->HidePreeditText();
}

void CandidateWindowControllerImpl::SetCursorLocation(
    const ibus::Rect& cursor_location,
    const ibus::Rect& composition_head) {
  // A workaround for http://crosbug.com/6460. We should ignore very short Y
  // move to prevent the window from shaking up and down.
  const int kKeepPositionThreshold = 2;  // px
  const gfx::Rect& last_location =
      candidate_window_->cursor_location();
  const int delta_y = abs(last_location.y() - cursor_location.y);
  if ((last_location.x() == cursor_location.x) &&
      (delta_y <= kKeepPositionThreshold)) {
    DVLOG(1) << "Ignored set_cursor_location signal to prevent window shake";
    return;
  }

  // Remember the cursor location.
  candidate_window_->set_cursor_location(IBusRectToGfxRect(cursor_location));
  candidate_window_->set_composition_head_location(
      IBusRectToGfxRect(composition_head));
  // Move the window per the cursor location.
  candidate_window_->ResizeAndMoveParentFrame();
  UpdateInfolistBounds();
}

void CandidateWindowControllerImpl::UpdateAuxiliaryText(
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

// static
void CandidateWindowControllerImpl::ConvertLookupTableToInfolistEntry(
    const IBusLookupTable& lookup_table,
    std::vector<InfolistWindowView::Entry>* infolist_entries,
    size_t* focused_index) {
  DCHECK(focused_index);
  DCHECK(infolist_entries);
  *focused_index = InfolistWindowView::InvalidFocusIndex();
  infolist_entries->clear();

  const size_t cursor_index_in_page =
      lookup_table.cursor_position() % lookup_table.page_size();

  for (size_t i = 0; i < lookup_table.candidates().size(); ++i) {
    const IBusLookupTable::Entry& ibus_entry =
        lookup_table.candidates()[i];
    if (ibus_entry.description_title.empty() &&
        ibus_entry.description_body.empty())
      continue;
    InfolistWindowView::Entry entry;
    entry.title = ibus_entry.description_title;
    entry.body = ibus_entry.description_body;
    infolist_entries->push_back(entry);
    if (i == cursor_index_in_page)
      *focused_index = infolist_entries->size() - 1;
  }
}

// static
bool CandidateWindowControllerImpl::ShouldUpdateInfolist(
    const std::vector<InfolistWindowView::Entry>& old_entries,
    size_t old_focused_index,
    const std::vector<InfolistWindowView::Entry>& new_entries,
    size_t new_focused_index) {
  if (old_entries.empty() && new_entries.empty())
    return false;
  if (old_entries.size() != new_entries.size())
    return true;
  if (old_focused_index != new_focused_index)
    return true;

  for (size_t i = 0; i < old_entries.size(); ++i) {
    if (old_entries[i].title != new_entries[i].title ||
        old_entries[i].body != new_entries[i].body ) {
      return true;
    }
  }
  return false;
}

void CandidateWindowControllerImpl::UpdateLookupTable(
    const IBusLookupTable& lookup_table,
    bool visible) {
  // If it's not visible, hide the lookup table and return.
  if (!visible) {
    candidate_window_->HideLookupTable();
    infolist_window_->Hide();
    // TODO(nona): Introduce unittests for crbug.com/170036.
    latest_infolist_entries_.clear();
    return;
  }

  candidate_window_->UpdateCandidates(lookup_table);
  candidate_window_->ShowLookupTable();

  size_t focused_index = 0;
  std::vector<InfolistWindowView::Entry> infolist_entries;
  ConvertLookupTableToInfolistEntry(lookup_table, &infolist_entries,
                                    &focused_index);

  // If there is no infolist entry, just hide.
  if (infolist_entries.empty()) {
    infolist_window_->Hide();
    return;
  }

  // If there is no change, just return.
  if (!ShouldUpdateInfolist(latest_infolist_entries_,
                            latest_infolist_focused_index_,
                            infolist_entries,
                            focused_index)) {
    return;
  }

  latest_infolist_entries_ = infolist_entries;
  latest_infolist_focused_index_ = focused_index;

  InfolistWindowView* view = static_cast<InfolistWindowView*>(
      infolist_window_->GetContentsView());
  if (!view) {
    DLOG(ERROR) << "Contents View is not InfolistWindowView.";
    return;
  }

  view->Relayout(infolist_entries, focused_index);
  UpdateInfolistBounds();

  if (focused_index < infolist_entries.size())
    infolist_window_->DelayShow(kInfolistShowDelayMilliSeconds);
  else
    infolist_window_->DelayHide(kInfolistHideDelayMilliSeconds);
}

void CandidateWindowControllerImpl::UpdateInfolistBounds() {
  InfolistWindowView* view = static_cast<InfolistWindowView*>(
      infolist_window_->GetContentsView());
  if (!view)
    return;
  const gfx::Rect current_bounds =
      infolist_window_->GetClientAreaBoundsInScreen();

  gfx::Rect new_bounds;
  new_bounds.set_size(view->GetPreferredSize());
  new_bounds.set_origin(GetInfolistWindowPosition(
        frame_->GetClientAreaBoundsInScreen(),
        ash::Shell::GetScreen()->GetDisplayNearestWindow(
            infolist_window_->GetNativeView()).work_area(),
        new_bounds.size()));

  if (current_bounds != new_bounds)
    infolist_window_->SetBounds(new_bounds);
}

void CandidateWindowControllerImpl::UpdatePreeditText(
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
  GetIBusPanelService()->CandidateClicked(
      index,
      static_cast<ibus::IBusMouseButton>(button),
      flags);
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

void CandidateWindowControllerImpl::OnConnected() {
  DBusThreadManager::Get()->GetIBusPanelService()->SetUpCandidateWindowHandler(
      this);
}

void CandidateWindowControllerImpl::OnDisconnected() {
  candidate_window_->HideAll();
  infolist_window_->Hide();
  DBusThreadManager::Get()->GetIBusPanelService()->SetUpCandidateWindowHandler(
      NULL);
}

// static
gfx::Point CandidateWindowControllerImpl::GetInfolistWindowPosition(
    const gfx::Rect& candidate_window_rect,
    const gfx::Rect& screen_rect,
    const gfx::Size& infolist_window_size) {
  gfx::Point result(candidate_window_rect.right(), candidate_window_rect.y());

  if (candidate_window_rect.right() + infolist_window_size.width() >
      screen_rect.right())
    result.set_x(candidate_window_rect.x() - infolist_window_size.width());

  if (candidate_window_rect.y() + infolist_window_size.height() >
      screen_rect.bottom())
    result.set_y(screen_rect.bottom() - infolist_window_size.height());

  return result;
}

}  // namespace input_method
}  // namespace chromeos
