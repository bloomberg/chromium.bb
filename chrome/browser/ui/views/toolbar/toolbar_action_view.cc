// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"

#include <string>

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/label_button_border.h"

using views::LabelButtonBorder;

namespace {

// We have smaller insets than normal STYLE_TEXTBUTTON buttons so that we can
// fit user supplied icons in without clipping them.
const int kBorderInset = 4;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ToolbarActionView

ToolbarActionView::ToolbarActionView(
    ToolbarActionViewController* view_controller,
    Profile* profile,
    ToolbarActionView::Delegate* delegate)
    : MenuButton(this, base::string16(), NULL, false),
      view_controller_(view_controller),
      profile_(profile),
      delegate_(delegate),
      called_register_command_(false),
      wants_to_run_(false) {
  set_id(VIEW_ID_BROWSER_ACTION);
  view_controller_->SetDelegate(this);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  if (view_controller_->CanDrag())
    set_drag_controller(delegate_);

  // We also listen for browser theme changes on linux because a switch from or
  // to GTK requires that we regrab our browser action images.
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));

  wants_to_run_border_ = CreateDefaultBorder();
  DecorateWantsToRunBorder(wants_to_run_border_.get());

  UpdateState();
}

ToolbarActionView::~ToolbarActionView() {
  view_controller_->SetDelegate(nullptr);
}

void ToolbarActionView::DecorateWantsToRunBorder(
    views::LabelButtonBorder* border) {
  // Create a special border for when the action wants to run, which gives the
  // button a "popped out" state.
  static const int kRaisedImages[] = IMAGE_GRID(IDR_TEXTBUTTON_RAISED);
  border->SetPainter(false,
                     views::Button::STATE_NORMAL,
                     views::Painter::CreateImageGridPainter(kRaisedImages));
}

gfx::Size ToolbarActionView::GetPreferredSize() const {
  return gfx::Size(ToolbarActionsBar::IconWidth(false),
                   ToolbarActionsBar::IconHeight());
}

const char* ToolbarActionView::GetClassName() const {
  return "ToolbarActionView";
}

void ToolbarActionView::OnDragDone() {
  views::MenuButton::OnDragDone();
  delegate_->OnToolbarActionViewDragDone();
}

void ToolbarActionView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && !called_register_command_ && GetFocusManager()) {
    view_controller_->RegisterCommand();
    called_register_command_ = true;
  }

  MenuButton::ViewHierarchyChanged(details);
}

void ToolbarActionView::PaintChildren(gfx::Canvas* canvas,
                                      const views::CullSet& cull_set) {
  View::PaintChildren(canvas, cull_set);
  view_controller_->PaintExtra(
      canvas, GetLocalBounds(), GetCurrentWebContents());
}

void ToolbarActionView::OnPaintBorder(gfx::Canvas* canvas) {
  if (!wants_to_run_)
    views::MenuButton::OnPaintBorder(canvas);
  else
    wants_to_run_border_->Paint(*this, canvas);
}

void ToolbarActionView::GetAccessibleState(ui::AXViewState* state) {
  views::MenuButton::GetAccessibleState(state);
  state->role = ui::AX_ROLE_BUTTON;
}

void ToolbarActionView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  view_controller_->ExecuteAction(true);
}

void ToolbarActionView::UpdateState() {
  content::WebContents* web_contents = GetCurrentWebContents();
  if (SessionTabHelper::IdForTab(web_contents) < 0)
    return;

  if (!view_controller_->IsEnabled(web_contents))
    SetState(views::CustomButton::STATE_DISABLED);
  else if (state() == views::CustomButton::STATE_DISABLED)
    SetState(views::CustomButton::STATE_NORMAL);

  wants_to_run_ = view_controller_->WantsToRun(web_contents);

  gfx::ImageSkia icon(view_controller_->GetIcon(web_contents).AsImageSkia());

  if (!icon.isNull()) {
    ThemeService* theme = ThemeServiceFactory::GetForProfile(profile_);

    gfx::ImageSkia bg = *theme->GetImageSkiaNamed(IDR_BROWSER_ACTION);
    SetImage(views::Button::STATE_NORMAL,
             gfx::ImageSkiaOperations::CreateSuperimposedImage(bg, icon));
  }

  SetTooltipText(view_controller_->GetTooltip(web_contents));
  SetAccessibleName(view_controller_->GetAccessibleName(web_contents));

  Layout();  // We need to layout since we may have added an icon as a result.
  SchedulePaint();
}

void ToolbarActionView::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  UpdateState();
}

bool ToolbarActionView::Activate() {
  if (!view_controller_->HasPopup(GetCurrentWebContents()))
    return true;

  view_controller_->ExecuteAction(true);

  // TODO(erikkay): Run a nested modal loop while the mouse is down to
  // enable menu-like drag-select behavior.

  // The return value of this method is returned via OnMousePressed.
  // We need to return false here since we're handing off focus to another
  // widget/view, and true will grab it right back and try to send events
  // to us.
  return false;
}

bool ToolbarActionView::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsRightMouseButton()) {
    return view_controller_->HasPopup(GetCurrentWebContents()) ?
        MenuButton::OnMousePressed(event) : LabelButton::OnMousePressed(event);
  }
  return false;
}

void ToolbarActionView::OnMouseReleased(const ui::MouseEvent& event) {
  if (view_controller_->HasPopup(GetCurrentWebContents()) ||
      view_controller_->IsMenuRunning()) {
    // TODO(erikkay) this never actually gets called (probably because of the
    // loss of focus).
    MenuButton::OnMouseReleased(event);
  } else {
    LabelButton::OnMouseReleased(event);
  }
}

void ToolbarActionView::OnMouseExited(const ui::MouseEvent& event) {
  if (view_controller_->HasPopup(GetCurrentWebContents()) ||
      view_controller_->IsMenuRunning())
    MenuButton::OnMouseExited(event);
  else
    LabelButton::OnMouseExited(event);
}

bool ToolbarActionView::OnKeyReleased(const ui::KeyEvent& event) {
  return view_controller_->HasPopup(GetCurrentWebContents()) ?
      MenuButton::OnKeyReleased(event) : LabelButton::OnKeyReleased(event);
}

void ToolbarActionView::OnGestureEvent(ui::GestureEvent* event) {
  if (view_controller_->HasPopup(GetCurrentWebContents()))
    MenuButton::OnGestureEvent(event);
  else
    LabelButton::OnGestureEvent(event);
}

scoped_ptr<LabelButtonBorder> ToolbarActionView::CreateDefaultBorder() const {
  scoped_ptr<LabelButtonBorder> border = LabelButton::CreateDefaultBorder();
  border->set_insets(gfx::Insets(kBorderInset, kBorderInset,
                                 kBorderInset, kBorderInset));
  return border.Pass();
}

bool ToolbarActionView::ShouldEnterPushedState(const ui::Event& event) {
  return view_controller_->HasPopup(GetCurrentWebContents()) ?
      MenuButton::ShouldEnterPushedState(event) :
      LabelButton::ShouldEnterPushedState(event);
}

gfx::ImageSkia ToolbarActionView::GetIconForTest() {
  return GetImage(views::Button::STATE_NORMAL);
}

views::View* ToolbarActionView::GetAsView() {
  return this;
}

bool ToolbarActionView::IsShownInMenu() {
  return delegate_->ShownInsideMenu();
}

views::FocusManager* ToolbarActionView::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::Widget* ToolbarActionView::GetParentForContextMenu() {
  // RunMenuAt expects a nested menu to be parented by the same widget as the
  // already visible menu, in this case the Chrome menu.
  return delegate_->ShownInsideMenu() ?
      delegate_->GetOverflowReferenceView()->GetWidget() :
      GetWidget();
}

ToolbarActionViewController*
ToolbarActionView::GetPreferredPopupViewController() {
  return delegate_->ShownInsideMenu() ?
      delegate_->GetMainViewForAction(this)->view_controller() :
      view_controller();
}

views::View* ToolbarActionView::GetReferenceViewForPopup() {
  // Browser actions in the overflow menu can still show popups, so we may need
  // a reference view other than this button's parent. If so, use the overflow
  // view.
  return visible() ? this : delegate_->GetOverflowReferenceView();
}

views::MenuButton* ToolbarActionView::GetContextMenuButton() {
  DCHECK(visible());  // We should never show a context menu for a hidden item.
  return this;
}

content::WebContents* ToolbarActionView::GetCurrentWebContents() const {
  return delegate_->GetCurrentWebContents();
}

void ToolbarActionView::OnPopupShown(bool by_user) {
  delegate_->SetPopupOwner(this);
  // If this was through direct user action, we press the menu button.
  if (by_user) {
    // We set the state of the menu button we're using as a reference view,
    // which is either this or the overflow reference view.
    // This cast is safe because GetReferenceViewForPopup returns either |this|
    // or delegate_->GetOverflowReferenceView(), which returns a MenuButton.
    views::MenuButton* reference_view =
        static_cast<views::MenuButton*>(GetReferenceViewForPopup());
    pressed_lock_.reset(new views::MenuButton::PressedLock(reference_view));
  }
}

void ToolbarActionView::OnPopupClosed() {
  delegate_->SetPopupOwner(nullptr);
  pressed_lock_.reset();  // Unpress the menu button if it was pressed.
}
