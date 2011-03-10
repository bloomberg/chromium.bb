// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/browser_actions_toolbar_gtk.h"

#include <algorithm>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/cairo_cached_surface.h"
#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_shrinkable_hbox.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/hover_controller_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/gtk_util.h"

namespace {

// The width of the browser action buttons.
const int kButtonWidth = 27;

// The padding between browser action buttons.
const int kButtonPadding = 4;

// The padding to the right of the browser action buttons (between the buttons
// and chevron if they are both showing).
const int kButtonChevronPadding = 2;

// The padding to the left, top and bottom of the browser actions toolbar
// separator.
const int kSeparatorPadding = 2;

// Width of the invisible gripper for resizing the toolbar.
const int kResizeGripperWidth = 4;

const char* kDragTarget = "application/x-chrome-browseraction";

GtkTargetEntry GetDragTargetEntry() {
  static std::string drag_target_string(kDragTarget);
  GtkTargetEntry drag_target;
  drag_target.target = const_cast<char*>(drag_target_string.c_str());
  drag_target.flags = GTK_TARGET_SAME_APP;
  drag_target.info = 0;
  return drag_target;
}

// The minimum width in pixels of the button hbox if |icon_count| icons are
// showing.
gint WidthForIconCount(gint icon_count) {
  return std::max((kButtonWidth + kButtonPadding) * icon_count - kButtonPadding,
                  0);
}

}  // namespace

using ui::SimpleMenuModel;

class BrowserActionButton : public NotificationObserver,
                            public ImageLoadingTracker::Observer,
                            public ExtensionContextMenuModel::PopupDelegate,
                            public MenuGtk::Delegate {
 public:
  BrowserActionButton(BrowserActionsToolbarGtk* toolbar,
                      const Extension* extension,
                      GtkThemeProvider* theme_provider)
      : toolbar_(toolbar),
        extension_(extension),
        image_(NULL),
        tracker_(this),
        tab_specific_icon_(NULL),
        default_icon_(NULL) {
    button_.reset(new CustomDrawButton(
        theme_provider,
        IDR_BROWSER_ACTION,
        IDR_BROWSER_ACTION_P,
        IDR_BROWSER_ACTION_H,
        0,
        NULL));
    alignment_.Own(gtk_alignment_new(0, 0, 1, 1));
    gtk_container_add(GTK_CONTAINER(alignment_.get()), button());
    gtk_widget_show(button());

    DCHECK(extension_->browser_action());

    UpdateState();

    // The Browser Action API does not allow the default icon path to be
    // changed at runtime, so we can load this now and cache it.
    std::string path = extension_->browser_action()->default_icon_path();
    if (!path.empty()) {
      tracker_.LoadImage(extension_, extension_->GetResource(path),
                         gfx::Size(Extension::kBrowserActionIconMaxSize,
                                   Extension::kBrowserActionIconMaxSize),
                         ImageLoadingTracker::DONT_CACHE);
    }

    signals_.Connect(button(), "button-press-event",
                     G_CALLBACK(OnButtonPress), this);
    signals_.Connect(button(), "clicked",
                     G_CALLBACK(OnClicked), this);
    signals_.Connect(button(), "drag-begin",
                     G_CALLBACK(&OnDragBegin), this);
    signals_.ConnectAfter(widget(), "expose-event",
                          G_CALLBACK(OnExposeEvent), this);

    registrar_.Add(this, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                   Source<ExtensionAction>(extension->browser_action()));
  }

  ~BrowserActionButton() {
    if (tab_specific_icon_)
      g_object_unref(tab_specific_icon_);

    if (default_icon_)
      g_object_unref(default_icon_);

    alignment_.Destroy();
  }

  GtkWidget* button() { return button_->widget(); }

  GtkWidget* widget() { return alignment_.get(); }

  const Extension* extension() { return extension_; }

  // NotificationObserver implementation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type == NotificationType::EXTENSION_BROWSER_ACTION_UPDATED)
      UpdateState();
    else
      NOTREACHED();
  }

  // ImageLoadingTracker::Observer implementation.
  void OnImageLoaded(SkBitmap* image, ExtensionResource resource, int index) {
    if (image) {
      default_skbitmap_ = *image;
      default_icon_ = gfx::GdkPixbufFromSkBitmap(image);
    }
    UpdateState();
  }

  // Updates the button based on the latest state from the associated
  // browser action.
  void UpdateState() {
    int tab_id = toolbar_->GetCurrentTabId();
    if (tab_id < 0)
      return;

    std::string tooltip = extension_->browser_action()->GetTitle(tab_id);
    if (tooltip.empty())
      gtk_widget_set_has_tooltip(button(), FALSE);
    else
      gtk_widget_set_tooltip_text(button(), tooltip.c_str());

    SkBitmap image = extension_->browser_action()->GetIcon(tab_id);
    if (!image.isNull()) {
      GdkPixbuf* previous_gdk_icon = tab_specific_icon_;
      tab_specific_icon_ = gfx::GdkPixbufFromSkBitmap(&image);
      SetImage(tab_specific_icon_);
      if (previous_gdk_icon)
        g_object_unref(previous_gdk_icon);
    } else if (default_icon_) {
      SetImage(default_icon_);
    }
    gtk_widget_queue_draw(button());
  }

  SkBitmap GetIcon() {
    const SkBitmap& image = extension_->browser_action()->GetIcon(
        toolbar_->GetCurrentTabId());
    if (!image.isNull()) {
      return image;
    } else {
      return default_skbitmap_;
    }
  }

  MenuGtk* GetContextMenu() {
    if (!extension_->ShowConfigureContextMenus())
      return NULL;

    context_menu_model_ =
        new ExtensionContextMenuModel(extension_, toolbar_->browser(), this);
    context_menu_.reset(
        new MenuGtk(this, context_menu_model_.get()));
    return context_menu_.get();
  }

 private:
  // MenuGtk::Delegate implementation.
  virtual void StoppedShowing() {
    button_->UnsetPaintOverride();

    // If the context menu was showing for the overflow menu, re-assert the
    // grab that was shadowed.
    if (toolbar_->overflow_menu_.get())
      gtk_util::GrabAllInput(toolbar_->overflow_menu_->widget());
  }

  virtual void CommandWillBeExecuted() {
    // If the context menu was showing for the overflow menu, and a command
    // is executed, then stop showing the overflow menu.
    if (toolbar_->overflow_menu_.get())
      toolbar_->overflow_menu_->Cancel();
  }

  // Returns true to prevent further processing of the event that caused us to
  // show the popup, or false to continue processing.
  bool ShowPopup(bool devtools) {
    ExtensionAction* browser_action = extension_->browser_action();

    int tab_id = toolbar_->GetCurrentTabId();
    if (tab_id < 0) {
      NOTREACHED() << "No current tab.";
      return true;
    }

    if (browser_action->HasPopup(tab_id)) {
      ExtensionPopupGtk::Show(
          browser_action->GetPopupUrl(tab_id), toolbar_->browser(),
          widget(), devtools);
      return true;
    }

    return false;
  }

  // ExtensionContextMenuModel::PopupDelegate implementation.
  virtual void InspectPopup(ExtensionAction* action) {
    ShowPopup(true);
  }

  void SetImage(GdkPixbuf* image) {
    if (!image_) {
      image_ = gtk_image_new_from_pixbuf(image);
      gtk_button_set_image(GTK_BUTTON(button()), image_);
    } else {
      gtk_image_set_from_pixbuf(GTK_IMAGE(image_), image);
    }
  }

  static gboolean OnButtonPress(GtkWidget* widget,
                                GdkEventButton* event,
                                BrowserActionButton* action) {
    if (event->button != 3)
      return FALSE;

    MenuGtk* menu = action->GetContextMenu();
    if (!menu)
      return FALSE;

    action->button_->SetPaintOverride(GTK_STATE_ACTIVE);
    menu->PopupForWidget(widget, event->button, event->time);

    return TRUE;
  }

  static void OnClicked(GtkWidget* widget, BrowserActionButton* action) {
    if (action->ShowPopup(false))
      return;

    ExtensionService* service =
        action->toolbar_->browser()->profile()->GetExtensionService();
    service->browser_event_router()->BrowserActionExecuted(
        action->toolbar_->browser()->profile(), action->extension_->id(),
        action->toolbar_->browser());
  }

  static gboolean OnExposeEvent(GtkWidget* widget,
                                GdkEventExpose* event,
                                BrowserActionButton* button) {
    int tab_id = button->toolbar_->GetCurrentTabId();
    if (tab_id < 0)
      return FALSE;

    ExtensionAction* action = button->extension_->browser_action();
    if (action->GetBadgeText(tab_id).empty())
      return FALSE;

    gfx::CanvasSkiaPaint canvas(event, false);
    gfx::Rect bounding_rect(widget->allocation);
    action->PaintBadge(&canvas, bounding_rect, tab_id);
    return FALSE;
  }

  static void OnDragBegin(GtkWidget* widget,
                          GdkDragContext* drag_context,
                          BrowserActionButton* button) {
    // Simply pass along the notification to the toolbar. The point of this
    // function is to tell the toolbar which BrowserActionButton initiated the
    // drag.
    button->toolbar_->DragStarted(button, drag_context);
  }

  // The toolbar containing this button.
  BrowserActionsToolbarGtk* toolbar_;

  // The extension that contains this browser action.
  const Extension* extension_;

  // The button for this browser action.
  scoped_ptr<CustomDrawButton> button_;

  // The top level widget (parent of |button_|).
  OwnedWidgetGtk alignment_;

  // The one image subwidget in |button_|. We keep this out so we don't alter
  // the widget hierarchy while changing the button image because changing the
  // GTK widget hierarchy invalidates all tooltips and several popular
  // extensions change browser action icon in a loop.
  GtkWidget* image_;

  // Loads the button's icons for us on the file thread.
  ImageLoadingTracker tracker_;

  // If we are displaying a tab-specific icon, it will be here.
  GdkPixbuf* tab_specific_icon_;

  // If the browser action has a default icon, it will be here.
  GdkPixbuf* default_icon_;

  // Same as |default_icon_|, but stored as SkBitmap.
  SkBitmap default_skbitmap_;

  ui::GtkSignalRegistrar signals_;
  NotificationRegistrar registrar_;

  // The context menu view and model for this extension action.
  scoped_ptr<MenuGtk> context_menu_;
  scoped_refptr<ExtensionContextMenuModel> context_menu_model_;

  friend class BrowserActionsToolbarGtk;
};

// BrowserActionsToolbarGtk ----------------------------------------------------

BrowserActionsToolbarGtk::BrowserActionsToolbarGtk(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      theme_provider_(GtkThemeProvider::GetFrom(browser->profile())),
      model_(NULL),
      hbox_(gtk_hbox_new(FALSE, 0)),
      button_hbox_(gtk_chrome_shrinkable_hbox_new(TRUE, FALSE, kButtonPadding)),
      drag_button_(NULL),
      drop_index_(-1),
      resize_animation_(this),
      desired_width_(0),
      start_width_(0),
      method_factory_(this) {
  ExtensionService* extension_service = profile_->GetExtensionService();
  // The |extension_service| can be NULL in Incognito.
  if (!extension_service)
    return;

  overflow_button_.reset(new CustomDrawButton(
      theme_provider_,
      IDR_BROWSER_ACTIONS_OVERFLOW,
      IDR_BROWSER_ACTIONS_OVERFLOW_P,
      IDR_BROWSER_ACTIONS_OVERFLOW_H,
      0,
      gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE)));

  GtkWidget* gripper = gtk_button_new();
  gtk_widget_set_size_request(gripper, kResizeGripperWidth, -1);
  GTK_WIDGET_UNSET_FLAGS(gripper, GTK_CAN_FOCUS);
  gtk_widget_add_events(gripper, GDK_POINTER_MOTION_MASK);
  signals_.Connect(gripper, "motion-notify-event",
                   G_CALLBACK(OnGripperMotionNotifyThunk), this);
  signals_.Connect(gripper, "expose-event",
                   G_CALLBACK(OnGripperExposeThunk), this);
  signals_.Connect(gripper, "enter-notify-event",
                   G_CALLBACK(OnGripperEnterNotifyThunk), this);
  signals_.Connect(gripper, "leave-notify-event",
                   G_CALLBACK(OnGripperLeaveNotifyThunk), this);
  signals_.Connect(gripper, "button-release-event",
                   G_CALLBACK(OnGripperButtonReleaseThunk), this);
  signals_.Connect(gripper, "button-press-event",
                   G_CALLBACK(OnGripperButtonPressThunk), this);
  signals_.Connect(chevron(), "button-press-event",
                   G_CALLBACK(OnOverflowButtonPressThunk), this);

  // |overflow_alignment| adds padding to the right of the browser action
  // buttons, but only appears when the overflow menu is showing.
  overflow_alignment_ = gtk_alignment_new(0, 0, 1, 1);
  gtk_container_add(GTK_CONTAINER(overflow_alignment_), chevron());

  // |overflow_area_| holds the overflow chevron and the separator, which
  // is only shown in GTK+ theme mode.
  overflow_area_ = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(overflow_area_), overflow_alignment_,
                     FALSE, FALSE, 0);

  separator_ = gtk_vseparator_new();
  gtk_box_pack_start(GTK_BOX(overflow_area_), separator_,
                     FALSE, FALSE, 0);
  gtk_widget_set_no_show_all(separator_, TRUE);

  gtk_widget_show_all(overflow_area_);
  gtk_widget_set_no_show_all(overflow_area_, TRUE);

  gtk_box_pack_start(GTK_BOX(hbox_.get()), gripper, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), button_hbox_.get(), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), overflow_area_, FALSE, FALSE, 0);

  model_ = extension_service->toolbar_model();
  model_->AddObserver(this);
  SetupDrags();

  if (model_->extensions_initialized()) {
    CreateAllButtons();
    SetContainerWidth();
  }

  // We want to connect to "set-focus" on the toplevel window; we have to wait
  // until we are added to a toplevel window to do so.
  signals_.Connect(widget(), "hierarchy-changed",
                   G_CALLBACK(OnHierarchyChangedThunk), this);

  ViewIDUtil::SetID(button_hbox_.get(), VIEW_ID_BROWSER_ACTION_TOOLBAR);

  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  theme_provider_->InitThemesFor(this);
}

BrowserActionsToolbarGtk::~BrowserActionsToolbarGtk() {
  if (model_)
    model_->RemoveObserver(this);
  button_hbox_.Destroy();
  hbox_.Destroy();
}

int BrowserActionsToolbarGtk::GetCurrentTabId() {
  TabContents* selected_tab = browser_->GetSelectedTabContents();
  if (!selected_tab)
    return -1;

  return selected_tab->controller().session_id().id();
}

void BrowserActionsToolbarGtk::Update() {
  for (ExtensionButtonMap::iterator iter = extension_button_map_.begin();
       iter != extension_button_map_.end(); ++iter) {
    iter->second->UpdateState();
  }
}

void BrowserActionsToolbarGtk::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  DCHECK(NotificationType::BROWSER_THEME_CHANGED == type);
  if (theme_provider_->UseGtkTheme())
    gtk_widget_show(separator_);
  else
    gtk_widget_hide(separator_);
}

void BrowserActionsToolbarGtk::SetupDrags() {
  GtkTargetEntry drag_target = GetDragTargetEntry();
  gtk_drag_dest_set(button_hbox_.get(), GTK_DEST_DEFAULT_DROP, &drag_target, 1,
                    GDK_ACTION_MOVE);

  signals_.Connect(button_hbox_.get(), "drag-motion",
                   G_CALLBACK(OnDragMotionThunk), this);
}

void BrowserActionsToolbarGtk::CreateAllButtons() {
  extension_button_map_.clear();

  int i = 0;
  for (ExtensionList::iterator iter = model_->begin();
       iter != model_->end(); ++iter) {
    CreateButtonForExtension(*iter, i++);
  }
}

void BrowserActionsToolbarGtk::SetContainerWidth() {
  int showing_actions = model_->GetVisibleIconCount();
  if (showing_actions >= 0)
    SetButtonHBoxWidth(WidthForIconCount(showing_actions));
}

void BrowserActionsToolbarGtk::CreateButtonForExtension(
    const Extension* extension, int index) {
  if (!ShouldDisplayBrowserAction(extension))
    return;

  if (profile_->IsOffTheRecord())
    index = model_->OriginalIndexToIncognito(index);

  RemoveButtonForExtension(extension);
  linked_ptr<BrowserActionButton> button(
      new BrowserActionButton(this, extension, theme_provider_));
  gtk_chrome_shrinkable_hbox_pack_start(
      GTK_CHROME_SHRINKABLE_HBOX(button_hbox_.get()), button->widget(), 0);
  gtk_box_reorder_child(GTK_BOX(button_hbox_.get()), button->widget(), index);
  extension_button_map_[extension->id()] = button;

  GtkTargetEntry drag_target = GetDragTargetEntry();
  gtk_drag_source_set(button->button(), GDK_BUTTON1_MASK, &drag_target, 1,
                      GDK_ACTION_MOVE);
  // We ignore whether the drag was a "success" or "failure" in Gtk's opinion.
  signals_.Connect(button->button(), "drag-end",
                   G_CALLBACK(&OnDragEndThunk), this);
  signals_.Connect(button->button(), "drag-failed",
                   G_CALLBACK(&OnDragFailedThunk), this);

  // Any time a browser action button is shown or hidden we have to update
  // the chevron state.
  signals_.Connect(button->widget(), "show",
                   G_CALLBACK(&OnButtonShowOrHideThunk), this);
  signals_.Connect(button->widget(), "hide",
                   G_CALLBACK(&OnButtonShowOrHideThunk), this);

  gtk_widget_show(button->widget());

  UpdateVisibility();
}

GtkWidget* BrowserActionsToolbarGtk::GetBrowserActionWidget(
    const Extension* extension) {
  ExtensionButtonMap::iterator it = extension_button_map_.find(
      extension->id());
  if (it == extension_button_map_.end())
    return NULL;

  return it->second.get()->widget();
}

void BrowserActionsToolbarGtk::RemoveButtonForExtension(
    const Extension* extension) {
  if (extension_button_map_.erase(extension->id()))
    UpdateVisibility();
  UpdateChevronVisibility();
}

void BrowserActionsToolbarGtk::UpdateVisibility() {
  if (button_count() == 0)
    gtk_widget_hide(widget());
  else
    gtk_widget_show(widget());
}

bool BrowserActionsToolbarGtk::ShouldDisplayBrowserAction(
    const Extension* extension) {
  // Only display incognito-enabled extensions while in incognito mode.
  return (!profile_->IsOffTheRecord() ||
          profile_->GetExtensionService()->IsIncognitoEnabled(extension));
}

void BrowserActionsToolbarGtk::HidePopup() {
  ExtensionPopupGtk* popup = ExtensionPopupGtk::get_current_extension_popup();
  if (popup)
    popup->DestroyPopup();
}

void BrowserActionsToolbarGtk::AnimateToShowNIcons(int count) {
  desired_width_ = WidthForIconCount(count);
  start_width_ = button_hbox_->allocation.width;
  resize_animation_.Reset();
  resize_animation_.Show();
}

void BrowserActionsToolbarGtk::BrowserActionAdded(const Extension* extension,
                                                  int index) {
  overflow_menu_.reset();

  CreateButtonForExtension(extension, index);

  // If we are still initializing the container, don't bother animating.
  if (!model_->extensions_initialized())
    return;

  // Animate the addition if we are showing all browser action buttons.
  if (!GTK_WIDGET_VISIBLE(overflow_area_)) {
    AnimateToShowNIcons(button_count());
    model_->SetVisibleIconCount(button_count());
  }
}

void BrowserActionsToolbarGtk::BrowserActionRemoved(
    const Extension* extension) {
  overflow_menu_.reset();

  if (drag_button_ != NULL) {
    // Break the current drag.
    gtk_grab_remove(button_hbox_.get());
  }

  RemoveButtonForExtension(extension);

  if (!GTK_WIDGET_VISIBLE(overflow_area_)) {
    AnimateToShowNIcons(button_count());
    model_->SetVisibleIconCount(button_count());
  }
}

void BrowserActionsToolbarGtk::BrowserActionMoved(const Extension* extension,
                                                  int index) {
  // We initiated this move action, and have already moved the button.
  if (drag_button_ != NULL)
    return;

  GtkWidget* button_widget = GetBrowserActionWidget(extension);
  if (!button_widget) {
    if (ShouldDisplayBrowserAction(extension))
      NOTREACHED();
    return;
  }

  if (profile_->IsOffTheRecord())
    index = model_->OriginalIndexToIncognito(index);

  gtk_box_reorder_child(GTK_BOX(button_hbox_.get()), button_widget, index);
}

void BrowserActionsToolbarGtk::ModelLoaded() {
  SetContainerWidth();
}

void BrowserActionsToolbarGtk::AnimationProgressed(
    const ui::Animation* animation) {
  int width = start_width_ + (desired_width_ - start_width_) *
      animation->GetCurrentValue();
  gtk_widget_set_size_request(button_hbox_.get(), width, -1);

  if (width == desired_width_)
    resize_animation_.Reset();
}

void BrowserActionsToolbarGtk::AnimationEnded(const ui::Animation* animation) {
  gtk_widget_set_size_request(button_hbox_.get(), desired_width_, -1);
  UpdateChevronVisibility();
}

bool BrowserActionsToolbarGtk::IsCommandIdChecked(int command_id) const {
  return false;
}

bool BrowserActionsToolbarGtk::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool BrowserActionsToolbarGtk::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void BrowserActionsToolbarGtk::ExecuteCommand(int command_id) {
  const Extension* extension = model_->GetExtensionByIndex(command_id);
  ExtensionAction* browser_action = extension->browser_action();

  int tab_id = GetCurrentTabId();
  if (tab_id < 0) {
    NOTREACHED() << "No current tab.";
    return;
  }

  if (browser_action->HasPopup(tab_id)) {
    ExtensionPopupGtk::Show(
        browser_action->GetPopupUrl(tab_id), browser(),
        chevron(),
        false);
  } else {
    ExtensionService* service = browser()->profile()->GetExtensionService();
    service->browser_event_router()->BrowserActionExecuted(
        browser()->profile(), extension->id(), browser());
  }
}

void BrowserActionsToolbarGtk::StoppedShowing() {
  overflow_button_->UnsetPaintOverride();
}

bool BrowserActionsToolbarGtk::AlwaysShowIconForCmd(int command_id) const {
  return true;
}

void BrowserActionsToolbarGtk::DragStarted(BrowserActionButton* button,
                                           GdkDragContext* drag_context) {
  // No representation of the widget following the cursor.
  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
  gtk_drag_set_icon_pixbuf(drag_context, pixbuf, 0, 0);
  g_object_unref(pixbuf);

  DCHECK(!drag_button_);
  drag_button_ = button;
}

void BrowserActionsToolbarGtk::SetButtonHBoxWidth(int new_width) {
  gint max_width = WidthForIconCount(button_count());
  new_width = std::min(max_width, new_width);
  new_width = std::max(new_width, 0);
  gtk_widget_set_size_request(button_hbox_.get(), new_width, -1);
}

void BrowserActionsToolbarGtk::UpdateChevronVisibility() {
  int showing_icon_count =
      gtk_chrome_shrinkable_hbox_get_visible_child_count(
          GTK_CHROME_SHRINKABLE_HBOX(button_hbox_.get()));
  if (showing_icon_count == 0) {
    gtk_alignment_set_padding(GTK_ALIGNMENT(overflow_alignment_), 0, 0, 0, 0);
  } else {
    gtk_alignment_set_padding(GTK_ALIGNMENT(overflow_alignment_), 0, 0,
                              kButtonChevronPadding, 0);
  }

  if (button_count() > showing_icon_count) {
    if (!GTK_WIDGET_VISIBLE(overflow_area_)) {
      if (drag_button_) {
        // During drags, when the overflow chevron shows for the first time,
        // take that much space away from |button_hbox_| to make the drag look
        // smoother.
        GtkRequisition req;
        gtk_widget_size_request(chevron(), &req);
        gint overflow_width = req.width;
        gtk_widget_size_request(button_hbox_.get(), &req);
        gint button_hbox_width = req.width;
        button_hbox_width = std::max(button_hbox_width - overflow_width, 0);
        gtk_widget_set_size_request(button_hbox_.get(), button_hbox_width, -1);
      }

      gtk_widget_show(overflow_area_);
    }
  } else {
    gtk_widget_hide(overflow_area_);
  }
}

gboolean BrowserActionsToolbarGtk::OnDragMotion(GtkWidget* widget,
                                                GdkDragContext* drag_context,
                                                gint x, gint y, guint time) {
  // Only handle drags we initiated.
  if (!drag_button_)
    return FALSE;

  if (base::i18n::IsRTL())
    x = widget->allocation.width - x;
  drop_index_ = x < kButtonWidth ? 0 : x / (kButtonWidth + kButtonPadding);

  // We will go ahead and reorder the child in order to provide visual feedback
  // to the user. We don't inform the model that it has moved until the drag
  // ends.
  gtk_box_reorder_child(GTK_BOX(button_hbox_.get()), drag_button_->widget(),
                        drop_index_);

  gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);
  return TRUE;
}

void BrowserActionsToolbarGtk::OnDragEnd(GtkWidget* button,
                                         GdkDragContext* drag_context) {
  if (drop_index_ != -1) {
    if (profile_->IsOffTheRecord())
      drop_index_ = model_->IncognitoIndexToOriginal(drop_index_);

    model_->MoveBrowserAction(drag_button_->extension(), drop_index_);
  }

  drag_button_ = NULL;
  drop_index_ = -1;
}

gboolean BrowserActionsToolbarGtk::OnDragFailed(GtkWidget* widget,
                                                GdkDragContext* drag_context,
                                                GtkDragResult result) {
  // We connect to this signal and return TRUE so that the default failure
  // animation (wherein the drag widget floats back to the start of the drag)
  // does not show, and the drag-end signal is emitted immediately instead of
  // several seconds later.
  return TRUE;
}

void BrowserActionsToolbarGtk::OnHierarchyChanged(
    GtkWidget* widget, GtkWidget* previous_toplevel) {
  GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
  if (!GTK_WIDGET_TOPLEVEL(toplevel))
    return;

  signals_.Connect(toplevel, "set-focus", G_CALLBACK(OnSetFocusThunk), this);
}

void BrowserActionsToolbarGtk::OnSetFocus(GtkWidget* widget,
                                          GtkWidget* focus_widget) {
  ExtensionPopupGtk* popup = ExtensionPopupGtk::get_current_extension_popup();
  // The focus of the parent window has changed. Close the popup. Delay the hide
  // because it will destroy the RenderViewHost, which may still be on the
  // call stack.
  if (!popup || popup->being_inspected())
    return;
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&BrowserActionsToolbarGtk::HidePopup));
}

gboolean BrowserActionsToolbarGtk::OnGripperMotionNotify(
    GtkWidget* widget, GdkEventMotion* event) {
  if (!(event->state & GDK_BUTTON1_MASK))
    return FALSE;

  // Calculate how much the user dragged the gripper and subtract that off the
  // button container's width.
  int distance_dragged = base::i18n::IsRTL() ?
      -event->x :
      event->x - widget->allocation.width;
  gint new_width = button_hbox_->allocation.width - distance_dragged;
  SetButtonHBoxWidth(new_width);

  return FALSE;
}

gboolean BrowserActionsToolbarGtk::OnGripperExpose(GtkWidget* gripper,
                                                   GdkEventExpose* expose) {
  return TRUE;
}

// These three signal handlers (EnterNotify, LeaveNotify, and ButtonRelease)
// are used to give the gripper the resize cursor. Since it doesn't have its
// own window, we have to set the cursor whenever the pointer moves into the
// button or leaves the button, and be sure to leave it on when the user is
// dragging.
gboolean BrowserActionsToolbarGtk::OnGripperEnterNotify(
    GtkWidget* gripper, GdkEventCrossing* event) {
  gdk_window_set_cursor(gripper->window,
                        gfx::GetCursor(GDK_SB_H_DOUBLE_ARROW));
  return FALSE;
}

gboolean BrowserActionsToolbarGtk::OnGripperLeaveNotify(
    GtkWidget* gripper, GdkEventCrossing* event) {
  if (!(event->state & GDK_BUTTON1_MASK))
    gdk_window_set_cursor(gripper->window, NULL);
  return FALSE;
}

gboolean BrowserActionsToolbarGtk::OnGripperButtonRelease(
    GtkWidget* gripper, GdkEventButton* event) {
  gfx::Rect gripper_rect(0, 0,
                         gripper->allocation.width, gripper->allocation.height);
  gfx::Point release_point(event->x, event->y);
  if (!gripper_rect.Contains(release_point))
    gdk_window_set_cursor(gripper->window, NULL);

  // After the user resizes the toolbar, we want to smartly resize it to be
  // the perfect size to fit the buttons.
  int visible_icon_count =
      gtk_chrome_shrinkable_hbox_get_visible_child_count(
          GTK_CHROME_SHRINKABLE_HBOX(button_hbox_.get()));
  AnimateToShowNIcons(visible_icon_count);
  model_->SetVisibleIconCount(visible_icon_count);

  return FALSE;
}

gboolean BrowserActionsToolbarGtk::OnGripperButtonPress(
    GtkWidget* gripper, GdkEventButton* event) {
  resize_animation_.Reset();

  return FALSE;
}

gboolean BrowserActionsToolbarGtk::OnOverflowButtonPress(
    GtkWidget* overflow, GdkEventButton* event) {
  overflow_menu_model_.reset(new SimpleMenuModel(this));

  int visible_icon_count =
      gtk_chrome_shrinkable_hbox_get_visible_child_count(
          GTK_CHROME_SHRINKABLE_HBOX(button_hbox_.get()));
  for (int i = visible_icon_count; i < button_count(); ++i) {
    int model_index = i;
    if (profile_->IsOffTheRecord())
      model_index = model_->IncognitoIndexToOriginal(i);

    const Extension* extension = model_->GetExtensionByIndex(model_index);
    BrowserActionButton* button = extension_button_map_[extension->id()].get();

    overflow_menu_model_->AddItem(model_index, UTF8ToUTF16(extension->name()));
    overflow_menu_model_->SetIcon(overflow_menu_model_->GetItemCount() - 1,
                                  button->GetIcon());

    // TODO(estade): set the menu item's tooltip.
  }

  overflow_menu_.reset(new MenuGtk(this, overflow_menu_model_.get()));
  signals_.Connect(overflow_menu_->widget(), "button-press-event",
                   G_CALLBACK(OnOverflowMenuButtonPressThunk), this);

  overflow_button_->SetPaintOverride(GTK_STATE_ACTIVE);
  overflow_menu_->PopupAsFromKeyEvent(chevron());

  return FALSE;
}

gboolean BrowserActionsToolbarGtk::OnOverflowMenuButtonPress(
    GtkWidget* overflow, GdkEventButton* event) {
  if (event->button != 3)
    return FALSE;

  GtkWidget* menu_item = GTK_MENU_SHELL(overflow)->active_menu_item;
  if (!menu_item)
    return FALSE;

  int item_index = g_list_index(GTK_MENU_SHELL(overflow)->children, menu_item);
  if (item_index == -1) {
    NOTREACHED();
    return FALSE;
  }

  item_index += gtk_chrome_shrinkable_hbox_get_visible_child_count(
      GTK_CHROME_SHRINKABLE_HBOX(button_hbox_.get()));
  if (profile_->IsOffTheRecord())
    item_index = model_->IncognitoIndexToOriginal(item_index);

  const Extension* extension = model_->GetExtensionByIndex(item_index);
  ExtensionButtonMap::iterator it = extension_button_map_.find(
      extension->id());
  if (it == extension_button_map_.end()) {
    NOTREACHED();
    return FALSE;
  }

  MenuGtk* menu = it->second.get()->GetContextMenu();
  if (!menu)
    return FALSE;

  menu->PopupAsContext(gfx::Point(event->x_root, event->y_root),
                       event->time);
  return TRUE;
}

void BrowserActionsToolbarGtk::OnButtonShowOrHide(GtkWidget* sender) {
  if (!resize_animation_.is_animating())
    UpdateChevronVisibility();
}
