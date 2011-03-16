// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_actions_container.h"

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/detachable_toolbar_view.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/drag_utils.h"
#include "views/metrics.h"
#include "views/window/window.h"

#include "grit/theme_resources.h"

// Horizontal spacing between most items in the container, as well as after the
// last item or chevron (if visible).
static const int kItemSpacing = ToolbarView::kStandardSpacing;
// Horizontal spacing before the chevron (if visible).
static const int kChevronSpacing = kItemSpacing - 2;

// static
bool BrowserActionsContainer::disable_animations_during_testing_ = false;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

BrowserActionButton::BrowserActionButton(const Extension* extension,
                                         BrowserActionsContainer* panel)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          MenuButton(this, std::wstring(), NULL, false)),
      browser_action_(extension->browser_action()),
      extension_(extension),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)),
      showing_context_menu_(false),
      panel_(panel) {
  set_border(NULL);
  set_alignment(TextButton::ALIGN_CENTER);

  // No UpdateState() here because View hierarchy not setup yet. Our parent
  // should call UpdateState() after creation.

  registrar_.Add(this, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                 Source<ExtensionAction>(browser_action_));
}

void BrowserActionButton::Destroy() {
  if (showing_context_menu_) {
    context_menu_menu_->CancelMenu();
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

void BrowserActionButton::ViewHierarchyChanged(
    bool is_add, View* parent, View* child) {
  if (is_add && child == this) {
    // The Browser Action API does not allow the default icon path to be
    // changed at runtime, so we can load this now and cache it.
    std::string relative_path = browser_action_->default_icon_path();
    if (relative_path.empty())
      return;

    // LoadImage is not guaranteed to be synchronous, so we might see the
    // callback OnImageLoaded execute immediately. It (through UpdateState)
    // expects parent() to return the owner for this button, so this
    // function is as early as we can start this request.
    tracker_.LoadImage(extension_, extension_->GetResource(relative_path),
                       gfx::Size(Extension::kBrowserActionIconMaxSize,
                                 Extension::kBrowserActionIconMaxSize),
                       ImageLoadingTracker::DONT_CACHE);
  }

  MenuButton::ViewHierarchyChanged(is_add, parent, child);
}

void BrowserActionButton::ButtonPressed(views::Button* sender,
                                        const views::Event& event) {
  panel_->OnBrowserActionExecuted(this, false);
}

void BrowserActionButton::OnImageLoaded(SkBitmap* image,
                                        const ExtensionResource& resource,
                                        int index) {
  if (image)
    default_icon_ = *image;

  // Call back to UpdateState() because a more specific icon might have been set
  // while the load was outstanding.
  UpdateState();
}

void BrowserActionButton::UpdateState() {
  int tab_id = panel_->GetCurrentTabId();
  if (tab_id < 0)
    return;

  SkBitmap icon(browser_action()->GetIcon(tab_id));
  if (icon.isNull())
    icon = default_icon_;
  if (!icon.isNull()) {
    SkPaint paint;
    paint.setXfermode(SkXfermode::Create(SkXfermode::kSrcOver_Mode));
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    SkBitmap bg;
    rb.GetBitmapNamed(IDR_BROWSER_ACTION)->copyTo(&bg,
        SkBitmap::kARGB_8888_Config);
    SkCanvas bg_canvas(bg);
    bg_canvas.drawBitmap(icon, SkIntToScalar((bg.width() - icon.width()) / 2),
        SkIntToScalar((bg.height() - icon.height()) / 2), &paint);
    SetIcon(bg);

    SkBitmap bg_h;
    rb.GetBitmapNamed(IDR_BROWSER_ACTION_H)->copyTo(&bg_h,
        SkBitmap::kARGB_8888_Config);
    SkCanvas bg_h_canvas(bg_h);
    bg_h_canvas.drawBitmap(icon,
        SkIntToScalar((bg_h.width() - icon.width()) / 2),
        SkIntToScalar((bg_h.height() - icon.height()) / 2), &paint);
    SetHoverIcon(bg_h);

    SkBitmap bg_p;
    rb.GetBitmapNamed(IDR_BROWSER_ACTION_P)->copyTo(&bg_p,
        SkBitmap::kARGB_8888_Config);
    SkCanvas bg_p_canvas(bg_p);
    bg_p_canvas.drawBitmap(icon,
        SkIntToScalar((bg_p.width() - icon.width()) / 2),
        SkIntToScalar((bg_p.height() - icon.height()) / 2), &paint);
    SetPushedIcon(bg_p);
  }

  // If the browser action name is empty, show the extension name instead.
  string16 name = UTF8ToUTF16(browser_action()->GetTitle(tab_id));
  if (name.empty())
    name = UTF8ToUTF16(extension()->name());
  SetTooltipText(UTF16ToWideHack(name));
  parent()->SchedulePaint();
}

bool BrowserActionButton::IsPopup() {
  int tab_id = panel_->GetCurrentTabId();
  return (tab_id < 0) ? false : browser_action_->HasPopup(tab_id);
}

GURL BrowserActionButton::GetPopupUrl() {
  int tab_id = panel_->GetCurrentTabId();
  return (tab_id < 0) ? GURL() : browser_action_->GetPopupUrl(tab_id);
}

void BrowserActionButton::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(type == NotificationType::EXTENSION_BROWSER_ACTION_UPDATED);
  UpdateState();
  // The browser action may have become visible/hidden so we need to make
  // sure the state gets updated.
  panel_->OnBrowserActionVisibilityChanged();
}

bool BrowserActionButton::Activate() {
  if (!IsPopup())
    return true;

  panel_->OnBrowserActionExecuted(this, false);

  // TODO(erikkay): Run a nested modal loop while the mouse is down to
  // enable menu-like drag-select behavior.

  // The return value of this method is returned via OnMousePressed.
  // We need to return false here since we're handing off focus to another
  // widget/view, and true will grab it right back and try to send events
  // to us.
  return false;
}

bool BrowserActionButton::OnMousePressed(const views::MouseEvent& event) {
  if (!event.IsRightMouseButton()) {
    return IsPopup() ?
        MenuButton::OnMousePressed(event) : TextButton::OnMousePressed(event);
  }

  // Get the top left point of this button in screen coordinates.
  gfx::Point point = gfx::Point(0, 0);
  ConvertPointToScreen(this, &point);

  // Make the menu appear below the button.
  point.Offset(0, height());

  ShowContextMenu(point, true);
  return false;
}

void BrowserActionButton::OnMouseReleased(const views::MouseEvent& event,
                                          bool canceled) {
  if (IsPopup() || showing_context_menu_) {
    // TODO(erikkay) this never actually gets called (probably because of the
    // loss of focus).
    MenuButton::OnMouseReleased(event, canceled);
  } else {
    TextButton::OnMouseReleased(event, canceled);
  }
}

void BrowserActionButton::OnMouseExited(const views::MouseEvent& event) {
  if (IsPopup() || showing_context_menu_)
    MenuButton::OnMouseExited(event);
  else
    TextButton::OnMouseExited(event);
}

bool BrowserActionButton::OnKeyReleased(const views::KeyEvent& event) {
  return IsPopup() ?
      MenuButton::OnKeyReleased(event) : TextButton::OnKeyReleased(event);
}

void BrowserActionButton::ShowContextMenu(const gfx::Point& p,
                                          bool is_mouse_gesture) {
  if (!extension()->ShowConfigureContextMenus())
    return;

  showing_context_menu_ = true;
  SetButtonPushed();

  // Reconstructs the menu every time because the menu's contents are dynamic.
  context_menu_contents_ =
      new ExtensionContextMenuModel(extension(), panel_->browser(), panel_);
  context_menu_menu_.reset(new views::Menu2(context_menu_contents_.get()));
  context_menu_menu_->RunContextMenuAt(p);

  SetButtonNotPushed();
  showing_context_menu_ = false;
}

void BrowserActionButton::SetButtonPushed() {
  SetState(views::CustomButton::BS_PUSHED);
  menu_visible_ = true;
}

void BrowserActionButton::SetButtonNotPushed() {
  SetState(views::CustomButton::BS_NORMAL);
  menu_visible_ = false;
}

BrowserActionButton::~BrowserActionButton() {
}


////////////////////////////////////////////////////////////////////////////////
// BrowserActionView

BrowserActionView::BrowserActionView(const Extension* extension,
                                     BrowserActionsContainer* panel)
    : panel_(panel) {
  button_ = new BrowserActionButton(extension, panel);
  button_->SetDragController(panel_);
  AddChildView(button_);
  button_->UpdateState();
}

BrowserActionView::~BrowserActionView() {
  RemoveChildView(button_);
  button_->Destroy();
}

gfx::Canvas* BrowserActionView::GetIconWithBadge() {
  int tab_id = panel_->GetCurrentTabId();

  SkBitmap icon = button_->extension()->browser_action()->GetIcon(tab_id);
  if (icon.isNull())
    icon = button_->default_icon();

  gfx::Canvas* canvas = new gfx::CanvasSkia(icon.width(), icon.height(), false);
  canvas->DrawBitmapInt(icon, 0, 0);

  if (tab_id >= 0) {
    gfx::Rect bounds(icon.width(), icon.height() + ToolbarView::kVertSpacing);
    button_->extension()->browser_action()->PaintBadge(canvas, bounds, tab_id);
  }

  return canvas;
}

void BrowserActionView::Layout() {
  // We can't rely on button_->GetPreferredSize() here because that's not set
  // correctly until the first call to
  // BrowserActionsContainer::RefreshBrowserActionViews(), whereas this can be
  // called before that when the initial bounds are set (and then not after,
  // since the bounds don't change).  So instead of setting the height from the
  // button's preferred size, we use IconHeight(), since that's how big the
  // button should be regardless of what it's displaying.
  button_->SetBounds(0, ToolbarView::kVertSpacing, width(),
                     BrowserActionsContainer::IconHeight());
}

void BrowserActionView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(
      IDS_ACCNAME_EXTENSIONS_BROWSER_ACTION);
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
}

void BrowserActionView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);
  ExtensionAction* action = button()->browser_action();
  int tab_id = panel_->GetCurrentTabId();
  if (tab_id >= 0)
    action->PaintBadge(canvas, gfx::Rect(width(), height()), tab_id);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer

BrowserActionsContainer::BrowserActionsContainer(Browser* browser,
                                                 View* owner_view)
    : profile_(browser->profile()),
      browser_(browser),
      owner_view_(owner_view),
      popup_(NULL),
      popup_button_(NULL),
      model_(NULL),
      container_width_(0),
      chevron_(NULL),
      overflow_menu_(NULL),
      suppress_chevron_(false),
      resize_amount_(0),
      animation_target_size_(0),
      drop_indicator_position_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(show_menu_task_factory_(this)) {
  SetID(VIEW_ID_BROWSER_ACTION_TOOLBAR);

  if (profile_->GetExtensionService()) {
    model_ = profile_->GetExtensionService()->toolbar_model();
    model_->AddObserver(this);
  }

  resize_animation_.reset(new ui::SlideAnimation(this));
  resize_area_ = new views::ResizeArea(this);
  AddChildView(resize_area_);

  chevron_ = new views::MenuButton(NULL, std::wstring(), this, false);
  chevron_->set_border(NULL);
  chevron_->EnableCanvasFlippingForRTLUI(true);
  chevron_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_EXTENSIONS_CHEVRON));
  chevron_->SetVisible(false);
  AddChildView(chevron_);
}

BrowserActionsContainer::~BrowserActionsContainer() {
  if (model_)
    model_->RemoveObserver(this);
  StopShowFolderDropMenuTimer();
  HidePopup();
  DeleteBrowserActionViews();
}

// Static.
void BrowserActionsContainer::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kBrowserActionContainerWidth, 0);
}

void BrowserActionsContainer::Init() {
  LoadImages();

  // We wait to set the container width until now so that the chevron images
  // will be loaded.  The width calculation needs to know the chevron size.
  if (model_ &&
      !profile_->GetPrefs()->HasPrefPath(prefs::kExtensionToolbarSize)) {
    // Migration code to the new VisibleIconCount pref.
    // TODO(mpcomplete): remove this after users are upgraded to 5.0.
    int predefined_width =
        profile_->GetPrefs()->GetInteger(prefs::kBrowserActionContainerWidth);
    if (predefined_width != 0)
      model_->SetVisibleIconCount(WidthToIconCount(predefined_width));
  }
  if (model_ && model_->extensions_initialized())
    SetContainerWidth();
}

int BrowserActionsContainer::GetCurrentTabId() const {
  TabContents* tab_contents = browser_->GetSelectedTabContents();
  return tab_contents ? tab_contents->controller().session_id().id() : -1;
}

BrowserActionView* BrowserActionsContainer::GetBrowserActionView(
    ExtensionAction* action) {
  for (BrowserActionViews::iterator iter = browser_action_views_.begin();
       iter != browser_action_views_.end(); ++iter) {
    if ((*iter)->button()->browser_action() == action)
      return *iter;
  }
  return NULL;
}

void BrowserActionsContainer::RefreshBrowserActionViews() {
  for (size_t i = 0; i < browser_action_views_.size(); ++i)
    browser_action_views_[i]->button()->UpdateState();
}

void BrowserActionsContainer::CreateBrowserActionViews() {
  DCHECK(browser_action_views_.empty());
  if (!model_)
    return;

  for (ExtensionList::iterator iter = model_->begin(); iter != model_->end();
       ++iter) {
    if (!ShouldDisplayBrowserAction(*iter))
      continue;

    BrowserActionView* view = new BrowserActionView(*iter, this);
    browser_action_views_.push_back(view);
    AddChildView(view);
  }
}

void BrowserActionsContainer::DeleteBrowserActionViews() {
  if (!browser_action_views_.empty()) {
    for (size_t i = 0; i < browser_action_views_.size(); ++i)
      RemoveChildView(browser_action_views_[i]);
    STLDeleteContainerPointers(browser_action_views_.begin(),
                               browser_action_views_.end());
    browser_action_views_.clear();
  }
}

void BrowserActionsContainer::OnBrowserActionVisibilityChanged() {
  SetVisible(!browser_action_views_.empty());
  owner_view_->Layout();
  owner_view_->SchedulePaint();
}

size_t BrowserActionsContainer::VisibleBrowserActions() const {
  size_t visible_actions = 0;
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    if (browser_action_views_[i]->IsVisible())
      ++visible_actions;
  }
  return visible_actions;
}

void BrowserActionsContainer::OnBrowserActionExecuted(
    BrowserActionButton* button,
    bool inspect_with_devtools) {
  ExtensionAction* browser_action = button->browser_action();

  // Popups just display.  No notification to the extension.
  // TODO(erikkay): should there be?
  if (!button->IsPopup()) {
    ExtensionService* service = profile_->GetExtensionService();
    service->browser_event_router()->BrowserActionExecuted(
        profile_, browser_action->extension_id(), browser_);
    return;
  }

  // If we're showing the same popup, just hide it and return.
  bool same_showing = popup_ && button == popup_button_;

  // Always hide the current popup, even if it's not the same.
  // Only one popup should be visible at a time.
  HidePopup();

  if (same_showing)
    return;

  // We can get the execute event for browser actions that are not visible,
  // since buttons can be activated from the overflow menu (chevron). In that
  // case we show the popup as originating from the chevron.
  View* reference_view = button->parent()->IsVisible() ? button : chevron_;
  gfx::Point origin;
  View::ConvertPointToScreen(reference_view, &origin);
  gfx::Rect rect = reference_view->bounds();
  rect.set_origin(origin);

  BubbleBorder::ArrowLocation arrow_location = base::i18n::IsRTL() ?
      BubbleBorder::TOP_LEFT : BubbleBorder::TOP_RIGHT;

  popup_ = ExtensionPopup::Show(button->GetPopupUrl(), browser_, rect,
                                arrow_location, inspect_with_devtools,
                                this);
  popup_button_ = button;
  popup_button_->SetButtonPushed();
}

gfx::Size BrowserActionsContainer::GetPreferredSize() {
  if (browser_action_views_.empty())
    return gfx::Size(ToolbarView::kStandardSpacing, 0);

  // We calculate the size of the view by taking the current width and
  // subtracting resize_amount_ (the latter represents how far the user is
  // resizing the view or, if animating the snapping, how far to animate it).
  // But we also clamp it to a minimum size and the maximum size, so that the
  // container can never shrink too far or take up more space than it needs. In
  // other words: ContainerMinSize() < width() - resize < ClampTo(MAX).
  int clamped_width = std::min(
      std::max(ContainerMinSize(), container_width_ - resize_amount_),
      IconCountToWidth(-1, false));
  return gfx::Size(clamped_width, 0);
}

void BrowserActionsContainer::Layout() {
  if (browser_action_views_.empty()) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  resize_area_->SetBounds(0, ToolbarView::kVertSpacing, kItemSpacing,
                          IconHeight());

  // If the icons don't all fit, show the chevron (unless suppressed).
  int max_x = GetPreferredSize().width();
  if ((IconCountToWidth(-1, false) > max_x) && !suppress_chevron_) {
    chevron_->SetVisible(true);
    gfx::Size chevron_size(chevron_->GetPreferredSize());
    max_x -=
        ToolbarView::kStandardSpacing + chevron_size.width() + kChevronSpacing;
    chevron_->SetBounds(
        width() - ToolbarView::kStandardSpacing - chevron_size.width(),
        ToolbarView::kVertSpacing, chevron_size.width(), chevron_size.height());
  } else {
    chevron_->SetVisible(false);
  }

  // Now draw the icons for the browser actions in the available space.
  int icon_width = IconWidth(false);
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    BrowserActionView* view = browser_action_views_[i];
    int x = ToolbarView::kStandardSpacing + (i * IconWidth(true));
    if (x + icon_width <= max_x) {
      view->SetBounds(x, 0, icon_width, height());
      view->SetVisible(true);
    } else {
      view->SetVisible(false);
    }
  }
}

bool BrowserActionsContainer::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  custom_formats->insert(BrowserActionDragData::GetBrowserActionCustomFormat());

  return true;
}

bool BrowserActionsContainer::AreDropTypesRequired() {
  return true;
}

bool BrowserActionsContainer::CanDrop(const OSExchangeData& data) {
  BrowserActionDragData drop_data;
  return drop_data.Read(data) ? drop_data.IsFromProfile(profile_) : false;
}

void BrowserActionsContainer::OnDragEntered(
    const views::DropTargetEvent& event) {
}

int BrowserActionsContainer::OnDragUpdated(
    const views::DropTargetEvent& event) {
  // First check if we are above the chevron (overflow) menu.
  if (GetEventHandlerForPoint(event.location()) == chevron_) {
    if (show_menu_task_factory_.empty() && !overflow_menu_)
      StartShowFolderDropMenuTimer();
    return ui::DragDropTypes::DRAG_MOVE;
  }
  StopShowFolderDropMenuTimer();

  // Figure out where to display the indicator.  This is a complex calculation:

  // First, we figure out how much space is to the left of the icon area, so we
  // can calculate the true offset into the icon area.
  int width_before_icons = ToolbarView::kStandardSpacing +
      (base::i18n::IsRTL() ?
          (chevron_->GetPreferredSize().width() + kChevronSpacing) : 0);
  int offset_into_icon_area = event.x() - width_before_icons;

  // Next, we determine which icon to place the indicator in front of.  We want
  // to place the indicator in front of icon n when the cursor is between the
  // midpoints of icons (n - 1) and n.  To do this we take the offset into the
  // icon area and transform it as follows:
  //
  // Real icon area:
  //   0   a     *  b        c
  //   |   |        |        |
  //   |[IC|ON]  [IC|ON]  [IC|ON]
  // We want to be before icon 0 for 0 < x <= a, icon 1 for a < x <= b, etc.
  // Here the "*" represents the offset into the icon area, and since it's
  // between a and b, we want to return "1".
  //
  // Transformed "icon area":
  //   0        a     *  b        c
  //   |        |        |        |
  //   |[ICON]  |[ICON]  |[ICON]  |
  // If we shift both our offset and our divider points later by half an icon
  // plus one spacing unit, then it becomes very easy to calculate how many
  // divider points we've passed, because they're the multiples of "one icon
  // plus padding".
  int before_icon_unclamped = (offset_into_icon_area + (IconWidth(false) / 2) +
      kItemSpacing) / IconWidth(true);

  // Because the user can drag outside the container bounds, we need to clamp to
  // the valid range.  Note that the maximum allowable value is (num icons), not
  // (num icons - 1), because we represent the indicator being past the last
  // icon as being "before the (last + 1) icon".
  int before_icon = std::min(std::max(before_icon_unclamped, 0),
                             static_cast<int>(VisibleBrowserActions()));

  // Now we convert back to a pixel offset into the container.  We want to place
  // the center of the drop indicator at the midpoint of the space before our
  // chosen icon.
  SetDropIndicator(width_before_icons + (before_icon * IconWidth(true)) -
      (kItemSpacing / 2));

  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::OnDragExited() {
  StopShowFolderDropMenuTimer();
  drop_indicator_position_ = -1;
  SchedulePaint();
}

int BrowserActionsContainer::OnPerformDrop(
    const views::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data()))
    return ui::DragDropTypes::DRAG_NONE;

  // Make sure we have the same view as we started with.
  DCHECK_EQ(browser_action_views_[data.index()]->button()->extension()->id(),
            data.id());
  DCHECK(model_);

  size_t i = 0;
  for (; i < browser_action_views_.size(); ++i) {
    int view_x = browser_action_views_[i]->GetMirroredBounds().x();
    if (!browser_action_views_[i]->IsVisible() ||
        (base::i18n::IsRTL() ? (view_x < drop_indicator_position_) :
            (view_x >= drop_indicator_position_))) {
      // We have reached the end of the visible icons or found one that has a
      // higher x position than the drop point.
      break;
    }
  }

  // |i| now points to the item to the right of the drop indicator*, which is
  // correct when dragging an icon to the left. When dragging to the right,
  // however, we want the icon being dragged to get the index of the item to
  // the left of the drop indicator, so we subtract one.
  // * Well, it can also point to the end, but not when dragging to the left. :)
  if (i > data.index())
    --i;

  if (profile_->IsOffTheRecord())
    i = model_->IncognitoIndexToOriginal(i);

  model_->MoveBrowserAction(
      browser_action_views_[data.index()]->button()->extension(), i);

  OnDragExited();  // Perform clean up after dragging.
  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_EXTENSIONS);
}

void BrowserActionsContainer::RunMenu(View* source, const gfx::Point& pt) {
  if (source == chevron_) {
    overflow_menu_ = new BrowserActionOverflowMenuController(
        this, chevron_, browser_action_views_, VisibleBrowserActions());
    overflow_menu_->set_observer(this);
    overflow_menu_->RunMenu(GetWindow()->GetNativeWindow(), false);
  }
}

void BrowserActionsContainer::WriteDragDataForView(View* sender,
                                                   const gfx::Point& press_pt,
                                                   OSExchangeData* data) {
  DCHECK(data);

  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    BrowserActionButton* button = browser_action_views_[i]->button();
    if (button == sender) {
      // Set the dragging image for the icon.
      scoped_ptr<gfx::Canvas> canvas(
          browser_action_views_[i]->GetIconWithBadge());
      drag_utils::SetDragImageOnDataObject(*canvas, button->size(), press_pt,
                                           data);

      // Fill in the remaining info.
      BrowserActionDragData drag_data(
          browser_action_views_[i]->button()->extension()->id(), i);
      drag_data.Write(profile_, data);
      break;
    }
  }
}

int BrowserActionsContainer::GetDragOperationsForView(View* sender,
                                                      const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool BrowserActionsContainer::CanStartDragForView(View* sender,
                                                  const gfx::Point& press_pt,
                                                  const gfx::Point& p) {
  return true;
}

void BrowserActionsContainer::OnResize(int resize_amount, bool done_resizing) {
  if (!done_resizing) {
    resize_amount_ = resize_amount;
    OnBrowserActionVisibilityChanged();
    return;
  }

  // Up until now we've only been modifying the resize_amount, but now it is
  // time to set the container size to the size we have resized to, and then
  // animate to the nearest icon count size if necessary (which may be 0).
  int max_width = IconCountToWidth(-1, false);
  container_width_ =
      std::min(std::max(0, container_width_ - resize_amount), max_width);
  SaveDesiredSizeAndAnimate(ui::Tween::EASE_OUT,
                            WidthToIconCount(container_width_));
}

void BrowserActionsContainer::AnimationProgressed(
    const ui::Animation* animation) {
  DCHECK_EQ(resize_animation_.get(), animation);
  resize_amount_ = static_cast<int>(resize_animation_->GetCurrentValue() *
      (container_width_ - animation_target_size_));
  OnBrowserActionVisibilityChanged();
}

void BrowserActionsContainer::AnimationEnded(const ui::Animation* animation) {
  container_width_ = animation_target_size_;
  animation_target_size_ = 0;
  resize_amount_ = 0;
  OnBrowserActionVisibilityChanged();
  suppress_chevron_ = false;
}

void BrowserActionsContainer::NotifyMenuDeleted(
    BrowserActionOverflowMenuController* controller) {
  DCHECK(controller == overflow_menu_);
  overflow_menu_ = NULL;
}

void BrowserActionsContainer::InspectPopup(ExtensionAction* action) {
  OnBrowserActionExecuted(GetBrowserActionView(action)->button(), true);
}

void BrowserActionsContainer::ExtensionPopupIsClosing(ExtensionPopup* popup) {
  // ExtensionPopup is ref-counted, so we don't need to delete it.
  DCHECK_EQ(popup_, popup);
  popup_ = NULL;
  popup_button_->SetButtonNotPushed();
  popup_button_ = NULL;
}

void BrowserActionsContainer::MoveBrowserAction(const std::string& extension_id,
                                                size_t new_index) {
  ExtensionService* service = profile_->GetExtensionService();
  if (service) {
    const Extension* extension = service->GetExtensionById(extension_id, false);
    model_->MoveBrowserAction(extension, new_index);
    SchedulePaint();
  }
}

void BrowserActionsContainer::HidePopup() {
  if (popup_)
    popup_->Close();
}

void BrowserActionsContainer::TestExecuteBrowserAction(int index) {
  BrowserActionButton* button = browser_action_views_[index]->button();
  OnBrowserActionExecuted(button, false);
}

void BrowserActionsContainer::TestSetIconVisibilityCount(size_t icons) {
  model_->SetVisibleIconCount(icons);
  chevron_->SetVisible(icons < browser_action_views_.size());
  container_width_ = IconCountToWidth(icons, chevron_->IsVisible());
  Layout();
  SchedulePaint();
}

void BrowserActionsContainer::OnPaint(gfx::Canvas* canvas) {
  // TODO(sky/glen): Instead of using a drop indicator, animate the icons while
  // dragging (like we do for tab dragging).
  if (drop_indicator_position_ > -1) {
    // The two-pixel width drop indicator.
    static const int kDropIndicatorWidth = 2;
    gfx::Rect indicator_bounds(
        drop_indicator_position_ - (kDropIndicatorWidth / 2),
        ToolbarView::kVertSpacing, kDropIndicatorWidth, IconHeight());

    // Color of the drop indicator.
    static const SkColor kDropIndicatorColor = SK_ColorBLACK;
    canvas->FillRectInt(kDropIndicatorColor, indicator_bounds.x(),
                        indicator_bounds.y(), indicator_bounds.width(),
                        indicator_bounds.height());
  }
}

void BrowserActionsContainer::OnThemeChanged() {
  LoadImages();
}

void BrowserActionsContainer::ViewHierarchyChanged(bool is_add,
                                                   views::View* parent,
                                                   views::View* child) {
  // No extensions (e.g., incognito).
  if (!model_)
    return;

  if (is_add && child == this) {
    // Initial toolbar button creation and placement in the widget hierarchy.
    // We do this here instead of in the constructor because AddBrowserAction
    // calls Layout on the Toolbar, which needs this object to be constructed
    // before its Layout function is called.
    CreateBrowserActionViews();
  }
}

// static
int BrowserActionsContainer::IconWidth(bool include_padding) {
  static bool initialized = false;
  static int icon_width = 0;
  if (!initialized) {
    initialized = true;
    icon_width = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_BROWSER_ACTION)->width();
  }
  return icon_width + (include_padding ? kItemSpacing : 0);
}

// static
int BrowserActionsContainer::IconHeight() {
  static bool initialized = false;
  static int icon_height = 0;
  if (!initialized) {
    initialized = true;
    icon_height = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_BROWSER_ACTION)->height();
  }
  return icon_height;
}

void BrowserActionsContainer::BrowserActionAdded(const Extension* extension,
                                                 int index) {
#if defined(DEBUG)
  for (size_t i = 0; i < browser_action_views_.size(); ++i) {
    DCHECK(browser_action_views_[i]->button()->extension() != extension) <<
           "Asked to add a browser action view for an extension that already "
           "exists.";
  }
#endif
  CloseOverflowMenu();

  if (!ShouldDisplayBrowserAction(extension))
    return;

  size_t visible_actions = VisibleBrowserActions();

  // Add the new browser action to the vector and the view hierarchy.
  if (profile_->IsOffTheRecord())
    index = model_->OriginalIndexToIncognito(index);
  BrowserActionView* view = new BrowserActionView(extension, this);
  browser_action_views_.insert(browser_action_views_.begin() + index, view);
  AddChildViewAt(view, index);

  // If we are still initializing the container, don't bother animating.
  if (!model_->extensions_initialized())
    return;

  // Enlarge the container if it was already at maximum size and we're not in
  // the middle of upgrading.
  if ((model_->GetVisibleIconCount() < 0) &&
      !profile_->GetExtensionService()->IsBeingUpgraded(extension)) {
    suppress_chevron_ = true;
    SaveDesiredSizeAndAnimate(ui::Tween::LINEAR, visible_actions + 1);
  } else {
    // Just redraw the (possibly modified) visible icon set.
    OnBrowserActionVisibilityChanged();
  }
}

void BrowserActionsContainer::BrowserActionRemoved(const Extension* extension) {
  CloseOverflowMenu();

  if (popup_ && popup_->host()->extension() == extension)
    HidePopup();

  size_t visible_actions = VisibleBrowserActions();
  for (BrowserActionViews::iterator iter = browser_action_views_.begin();
       iter != browser_action_views_.end(); ++iter) {
    if ((*iter)->button()->extension() == extension) {
      RemoveChildView(*iter);
      delete *iter;
      browser_action_views_.erase(iter);

      // If the extension is being upgraded we don't want the bar to shrink
      // because the icon is just going to get re-added to the same location.
      if (profile_->GetExtensionService()->IsBeingUpgraded(extension))
        return;

      if (browser_action_views_.size() > visible_actions) {
        // If we have more icons than we can show, then we must not be changing
        // the container size (since we either removed an icon from the main
        // area and one from the overflow list will have shifted in, or we
        // removed an entry directly from the overflow list).
        OnBrowserActionVisibilityChanged();
      } else {
        // Either we went from overflow to no-overflow, or we shrunk the no-
        // overflow container by 1.  Either way the size changed, so animate.
        chevron_->SetVisible(false);
        SaveDesiredSizeAndAnimate(ui::Tween::EASE_OUT,
                                  browser_action_views_.size());
      }
      return;
    }
  }
}

void BrowserActionsContainer::BrowserActionMoved(const Extension* extension,
                                                 int index) {
  if (!ShouldDisplayBrowserAction(extension))
    return;

  if (profile_->IsOffTheRecord())
    index = model_->OriginalIndexToIncognito(index);

  DCHECK(index >= 0 && index < static_cast<int>(browser_action_views_.size()));

  DeleteBrowserActionViews();
  CreateBrowserActionViews();
  Layout();
  SchedulePaint();
}

void BrowserActionsContainer::ModelLoaded() {
  SetContainerWidth();
}

void BrowserActionsContainer::LoadImages() {
  ui::ThemeProvider* tp = GetThemeProvider();
  chevron_->SetIcon(*tp->GetBitmapNamed(IDR_BROWSER_ACTIONS_OVERFLOW));
  chevron_->SetHoverIcon(*tp->GetBitmapNamed(IDR_BROWSER_ACTIONS_OVERFLOW_H));
  chevron_->SetPushedIcon(*tp->GetBitmapNamed(IDR_BROWSER_ACTIONS_OVERFLOW_P));
}

void BrowserActionsContainer::SetContainerWidth() {
  int visible_actions = model_->GetVisibleIconCount();
  if (visible_actions < 0)  // All icons should be visible.
    visible_actions = model_->size();
  chevron_->SetVisible(static_cast<size_t>(visible_actions) < model_->size());
  container_width_ = IconCountToWidth(visible_actions, chevron_->IsVisible());
}

void BrowserActionsContainer::CloseOverflowMenu() {
  if (overflow_menu_)
    overflow_menu_->CancelMenu();
}

void BrowserActionsContainer::StopShowFolderDropMenuTimer() {
  show_menu_task_factory_.RevokeAll();
}

void BrowserActionsContainer::StartShowFolderDropMenuTimer() {
  int delay = views::GetMenuShowDelay();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      show_menu_task_factory_.NewRunnableMethod(
          &BrowserActionsContainer::ShowDropFolder),
      delay);
}

void BrowserActionsContainer::ShowDropFolder() {
  DCHECK(!overflow_menu_);
  SetDropIndicator(-1);
  overflow_menu_ = new BrowserActionOverflowMenuController(
      this, chevron_, browser_action_views_, VisibleBrowserActions());
  overflow_menu_->set_observer(this);
  overflow_menu_->RunMenu(GetWindow()->GetNativeWindow(), true);
}

void BrowserActionsContainer::SetDropIndicator(int x_pos) {
  if (drop_indicator_position_ != x_pos) {
    drop_indicator_position_ = x_pos;
    SchedulePaint();
  }
}

int BrowserActionsContainer::IconCountToWidth(int icons,
                                              bool display_chevron) const {
  if (icons < 0)
    icons = browser_action_views_.size();
  if ((icons == 0) && !display_chevron)
    return ToolbarView::kStandardSpacing;
  int icons_size =
      (icons == 0) ? 0 : ((icons * IconWidth(true)) - kItemSpacing);
  int chevron_size = display_chevron ?
      (kChevronSpacing + chevron_->GetPreferredSize().width()) : 0;
  return ToolbarView::kStandardSpacing + icons_size + chevron_size +
      ToolbarView::kStandardSpacing;
}

size_t BrowserActionsContainer::WidthToIconCount(int pixels) const {
  // Check for widths large enough to show the entire icon set.
  if (pixels >= IconCountToWidth(-1, false))
    return browser_action_views_.size();

  // We need to reserve space for the resize area, chevron, and the spacing on
  // either side of the chevron.
  int available_space = pixels - ToolbarView::kStandardSpacing -
      chevron_->GetPreferredSize().width() - kChevronSpacing -
      ToolbarView::kStandardSpacing;
  // Now we add an extra between-item padding value so the space can be divided
  // evenly by (size of icon with padding).
  return static_cast<size_t>(
      std::max(0, available_space + kItemSpacing) / IconWidth(true));
}

int BrowserActionsContainer::ContainerMinSize() const {
  return ToolbarView::kStandardSpacing + kChevronSpacing +
      chevron_->GetPreferredSize().width() + ToolbarView::kStandardSpacing;
}

void BrowserActionsContainer::SaveDesiredSizeAndAnimate(
    ui::Tween::Type tween_type,
    size_t num_visible_icons) {
  // Save off the desired number of visible icons.  We do this now instead of at
  // the end of the animation so that even if the browser is shut down while
  // animating, the right value will be restored on next run.
  // NOTE: Don't save the icon count in incognito because there may be fewer
  // icons in that mode. The result is that the container in a normal window is
  // always at least as wide as in an incognito window.
  if (!profile_->IsOffTheRecord())
    model_->SetVisibleIconCount(num_visible_icons);

  int target_size = IconCountToWidth(num_visible_icons,
      num_visible_icons < browser_action_views_.size());
  if (!disable_animations_during_testing_) {
    // Animate! We have to set the animation_target_size_ after calling Reset(),
    // because that could end up calling AnimationEnded which clears the value.
    resize_animation_->Reset();
    resize_animation_->SetTweenType(tween_type);
    animation_target_size_ = target_size;
    resize_animation_->Show();
  } else {
    animation_target_size_ = target_size;
    AnimationEnded(resize_animation_.get());
  }
}

bool BrowserActionsContainer::ShouldDisplayBrowserAction(
    const Extension* extension) {
  // Only display incognito-enabled extensions while in incognito mode.
  return (!profile_->IsOffTheRecord() ||
          profile_->GetExtensionService()->IsIncognitoEnabled(extension));
}
