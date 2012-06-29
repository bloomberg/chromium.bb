// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/reload_button.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/event_disposition.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/metrics.h"

// static
const char ReloadButton::kViewClassName[] = "browser/ui/views/ReloadButton";

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, public:

ReloadButton::ReloadButton(LocationBarView* location_bar,
                           CommandUpdater* command_updater)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ToggleImageButton(this)),
      location_bar_(location_bar),
      command_updater_(command_updater),
      intended_mode_(MODE_RELOAD),
      visible_mode_(MODE_RELOAD),
      double_click_timer_delay_(
          base::TimeDelta::FromMilliseconds(views::GetDoubleClickInterval())),
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
    // For instant extended API, if mode is NTP, disable button state.
    bool disabled = location_bar_ && location_bar_->search_model() &&
        chrome::search::IsInstantExtendedAPIEnabled(location_bar_->profile()) &&
        location_bar_->search_model()->mode().is_ntp();
    SetEnabled(!disabled);

  // We want to disable the button if we're preventing a change from stop to
  // reload due to hovering, but not if we're preventing a change from reload to
  // stop due to the double-click timer running.  (Disabled reload state is only
  // applicable when instant extended API is enabled and mode is NTP, which is
  // handled just above.)
  } else if (visible_mode_ != MODE_RELOAD) {
    SetEnabled(false);

    // Go ahead and change to reload after a bit, which allows repeated reloads
    // without moving the mouse.
    if (!stop_to_reload_timer_.IsRunning()) {
      stop_to_reload_timer_.Start(FROM_HERE, stop_to_reload_timer_delay_, this,
                                  &ReloadButton::OnStopToReloadTimer);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, views::ButtonListener implementation:

void ReloadButton::ButtonPressed(views::Button* /* button */,
                                 const views::Event& event) {
  if (visible_mode_ == MODE_STOP) {
    if (command_updater_)
      command_updater_->ExecuteCommandWithDisposition(IDC_STOP, CURRENT_TAB);
    // The user has clicked, so we can feel free to update the button,
    // even if the mouse is still hovering.
    ChangeMode(MODE_RELOAD, true);
  } else if (!double_click_timer_.IsRunning()) {
    // Shift-clicking or ctrl-clicking the reload button means we should ignore
    // any cached content.
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
        browser::DispositionFromEventFlags(flags);
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
    double_click_timer_.Start(FROM_HERE, double_click_timer_delay_, this,
                              &ReloadButton::OnDoubleClickTimer);

    if (command_updater_)
      command_updater_->ExecuteCommandWithDisposition(command, disposition);
    ++testing_reload_count_;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, View overrides:

void ReloadButton::OnMouseExited(const views::MouseEvent& event) {
  ChangeMode(intended_mode_, true);
  if (state() != BS_DISABLED)
    SetState(BS_NORMAL);
}

bool ReloadButton::GetTooltipText(const gfx::Point& p,
                                  string16* tooltip) const {
  int text_id = (visible_mode_ == MODE_RELOAD) ?
      IDS_TOOLTIP_RELOAD : IDS_TOOLTIP_STOP;
  tooltip->assign(l10n_util::GetStringUTF16(text_id));
  return true;
}

std::string ReloadButton::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// ReloadButton, private:

void ReloadButton::OnDoubleClickTimer() {
  ChangeMode(intended_mode_, false);
}

void ReloadButton::OnStopToReloadTimer() {
  ChangeMode(intended_mode_, true);
}
