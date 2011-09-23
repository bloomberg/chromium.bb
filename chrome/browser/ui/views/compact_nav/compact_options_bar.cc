// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compact_nav/compact_options_bar.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/views/wrench_menu.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_listener.h"

namespace {

const int kPreferredHeight = 25;
// Pad the left and right ends from other tabstrip region items.
const int kEndPadding = 3;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CompactOptionsBar public:

CompactOptionsBar::CompactOptionsBar(BrowserView* browser_view)
    : browser_view_(browser_view),
      initialized_(false),
      app_menu_(NULL) {
}

CompactOptionsBar::~CompactOptionsBar() {
}

void CompactOptionsBar::Init() {
  DCHECK(!initialized_);
  initialized_ = true;

  wrench_menu_model_.reset(new WrenchMenuModel(this, browser_view_->browser()));

  app_menu_ = new views::MenuButton(NULL, std::wstring(), this, false);
  app_menu_->set_border(NULL);
  app_menu_->EnableCanvasFlippingForRTLUI(true);
  app_menu_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_APP));
  app_menu_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_APPMENU_TOOLTIP,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  app_menu_->set_id(VIEW_ID_APP_MENU);
  AddChildView(app_menu_);

  LoadImages();
}

void CompactOptionsBar::AddMenuListener(views::MenuListener* listener) {
  menu_listeners_.push_back(listener);
}

void CompactOptionsBar::RemoveMenuListener(views::MenuListener* listener) {
  for (std::vector<views::MenuListener*>::iterator i(menu_listeners_.begin());
       i != menu_listeners_.end(); ++i) {
    if (*i == listener) {
      menu_listeners_.erase(i);
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// CompactOptionsBar, views::View overrides:

gfx::Size CompactOptionsBar::GetPreferredSize() {
  int width = kEndPadding * 2 + app_menu_->GetPreferredSize().width();
  // TODO(stevet): Add the width of the browser actions container here when that
  // is added to the bar.
  return gfx::Size(width, kPreferredHeight);
}

void CompactOptionsBar::Layout() {
  if (!initialized_)
    return;

  gfx::Size button_size = app_menu_->GetPreferredSize();
  app_menu_->SetBounds(kEndPadding, 0, button_size.width(), height());
}

////////////////////////////////////////////////////////////////////////////////
// CompactOptionsBar, views::MenuDelegate implementation:

void CompactOptionsBar::RunMenu(views::View* source,
                                const gfx::Point& /* pt */) {
  DCHECK_EQ(VIEW_ID_APP_MENU, source->id());

  wrench_menu_.reset(new WrenchMenu(browser_view_->browser()));
  wrench_menu_->Init(wrench_menu_model_.get());

  for (size_t i = 0; i < menu_listeners_.size(); ++i)
    menu_listeners_[i]->OnMenuOpened();

  // Note that this might be destroyed while the menu is running.
  wrench_menu_->RunMenu(app_menu_);
}

////////////////////////////////////////////////////////////////////////////////
// CompactOptionsBar, ui::AcceleratorProvider implementation:

bool CompactOptionsBar::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  // TODO(stevet): Can we share ToolbarView's implementation? It's exactly the
  // same so far.
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  // TODO(cpu) Bug 1109102. Query WebKit land for the actual bindings.
  switch (command_id) {
    case IDC_CUT:
      *accelerator = views::Accelerator(ui::VKEY_X, false, true, false);
      return true;
    case IDC_COPY:
      *accelerator = views::Accelerator(ui::VKEY_C, false, true, false);
      return true;
    case IDC_PASTE:
      *accelerator = views::Accelerator(ui::VKEY_V, false, true, false);
      return true;
  }
  // Else, we retrieve the accelerator information from the frame.
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// CompactOptionsBar, views::ButtonListener implementation.

void CompactOptionsBar::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  browser_view_->browser()->ExecuteCommandWithDisposition(
      sender->tag(),
      event_utils::DispositionFromEventFlags(sender->mouse_event_flags()));
}

void CompactOptionsBar::LoadImages() {
  // Reuse the resources loaded for the Toolbar's AppMenu.
  DCHECK(browser_view_->toolbar());
  app_menu_->SetIcon(browser_view_->toolbar()->GetAppMenuIcon(
      views::CustomButton::BS_NORMAL));
  app_menu_->SetHoverIcon(browser_view_->toolbar()->GetAppMenuIcon(
      views::CustomButton::BS_HOT));
  app_menu_->SetPushedIcon(browser_view_->toolbar()->GetAppMenuIcon(
      views::CustomButton::BS_PUSHED));
}
