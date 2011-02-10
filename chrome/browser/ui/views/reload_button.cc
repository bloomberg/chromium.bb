// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/reload_button.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, public:

ReloadButton::ReloadButton(LocationBarView* location_bar, Browser* browser)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ToggleImageButton(this)),
      location_bar_(location_bar),
      browser_(browser),
      intended_mode_(MODE_RELOAD),
      visible_mode_(MODE_RELOAD),
      double_click_timer_delay_(
          base::TimeDelta::FromMilliseconds(GetDoubleClickTimeMS())),
      stop_to_reload_timer_delay_(base::TimeDelta::FromMilliseconds(1350)),
      testing_mouse_hovered_(false),
      testing_reload_count_(0) {
}

ReloadButton::~ReloadButton() {
}

void ReloadButton::ChangeMode(Mode mode, bool force) {
  intended_mode_ = mode;

  // If the change is forced, or the user isn't hovering the icon, or it's safe
  // to change it to the other image type, make the change immediately;
  // otherwise we'll let it happen later.
  if (force || (!IsMouseHovered() && !testing_mouse_hovered_) ||
      ((mode == MODE_STOP) ?
      !double_click_timer_.IsRunning() : (visible_mode_ != MODE_STOP))) {
    double_click_timer_.Stop();
    stop_to_reload_timer_.Stop();
    SetToggled(mode == MODE_STOP);
    visible_mode_ = mode;
    SetEnabled(true);

  // We want to disable the button if we're preventing a change from stop to
  // reload due to hovering, but not if we're preventing a change from reload to
  // stop due to the double-click timer running.  (There is no disabled reload
  // state.)
  } else if (visible_mode_ != MODE_RELOAD) {
    SetEnabled(false);

    // Go ahead and change to reload after a bit, which allows repeated reloads
    // without moving the mouse.
    if (!stop_to_reload_timer_.IsRunning()) {
      stop_to_reload_timer_.Start(stop_to_reload_timer_delay_, this,
                                  &ReloadButton::OnStopToReloadTimer);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, views::ButtonListener implementation:

void ReloadButton::ButtonPressed(views::Button* /* button */,
                                 const views::Event& event) {
  if (visible_mode_ == MODE_STOP) {
    if (browser_)
      browser_->Stop();
    // The user has clicked, so we can feel free to update the button,
    // even if the mouse is still hovering.
    ChangeMode(MODE_RELOAD, true);
  } else if (!double_click_timer_.IsRunning()) {
    // Shift-clicking or ctrl-clicking the reload button means we should ignore
    // any cached content.
    // TODO(avayvod): eliminate duplication of this logic in
    // CompactLocationBarView.
    int command;
    int flags = mouse_event_flags();
    if (event.IsShiftDown() || event.IsControlDown()) {
      command = IDC_RELOAD_IGNORING_CACHE;
      // Mask off Shift and Control so they don't affect the disposition below.
      flags &= ~(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
    } else {
      command = IDC_RELOAD;
    }

    WindowOpenDisposition disposition =
        event_utils::DispositionFromEventFlags(flags);
    if ((disposition == CURRENT_TAB) && location_bar_) {
      // Forcibly reset the location bar, since otherwise it won't discard any
      // ongoing user edits, since it doesn't realize this is a user-initiated
      // action.
      location_bar_->Revert();
    }

    // Start a timer - while this timer is running, the reload button cannot be
    // changed to a stop button.  We do not set |intended_mode_| to MODE_STOP
    // here as the browser will do that when it actually starts loading (which
    // may happen synchronously, thus the need to do this before telling the
    // browser to execute the reload command).
    double_click_timer_.Start(double_click_timer_delay_, this,
                              &ReloadButton::OnDoubleClickTimer);

    if (browser_)
      browser_->ExecuteCommandWithDisposition(command, disposition);
    ++testing_reload_count_;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, View overrides:

void ReloadButton::OnMouseExited(const views::MouseEvent& e) {
  ChangeMode(intended_mode_, true);
  if (state() != BS_DISABLED)
    SetState(BS_NORMAL);
}

bool ReloadButton::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  int text_id = visible_mode_ == MODE_RELOAD ? IDS_TOOLTIP_RELOAD
                                             : IDS_TOOLTIP_STOP;
  tooltip->assign(UTF16ToWide(l10n_util::GetStringUTF16(text_id)));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, private:

void ReloadButton::OnDoubleClickTimer() {
  ChangeMode(intended_mode_, false);
}

void ReloadButton::OnStopToReloadTimer() {
  ChangeMode(intended_mode_, true);
}
