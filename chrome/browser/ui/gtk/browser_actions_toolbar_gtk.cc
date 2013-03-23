// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/browser_actions_toolbar_gtk.h"

#include <gtk/gtk.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_shrinkable_hbox.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/hover_controller_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/accelerators/platform_accelerator_gtk.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::Extension;
using extensions::ExtensionActionManager;

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

const char kDragTarget[] = "application/x-chrome-browseraction";

GtkTargetEntry GetDragTargetEntry() {
  GtkTargetEntry drag_target;
  drag_target.target = const_cast<char*>(kDragTarget);
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

class BrowserActionButton : public content::NotificationObserver,
                            public ExtensionActionIconFactory::Observer,
                            public ExtensionContextMenuModel::PopupDelegate,
                            public MenuGtk::Delegate {
 public:
  BrowserActionButton(BrowserActionsToolbarGtk* toolbar,
                      const Extension* extension,
                      GtkThemeService* theme_provider)
      : toolbar_(toolbar),
        extension_(extension),
        image_(NULL),
        icon_factory_(toolbar->browser()->profile(), extension,
                      browser_action(), this),
        accel_group_(NULL) {
    button_.reset(new CustomDrawButton(
        theme_provider,
        IDR_BROWSER_ACTION,
        IDR_BROWSER_ACTION_P,
        IDR_BROWSER_ACTION_H,
        0,
        NULL));
    gtk_widget_set_size_request(button(), kButtonWidth, kButtonWidth);
    alignment_.Own(gtk_alignment_new(0, 0, 1, 1));
    gtk_container_add(GTK_CONTAINER(alignment_.get()), button());
    gtk_widget_show(button());

    DCHECK(browser_action());

    UpdateState();

    signals_.Connect(button(), "button-press-event",
                     G_CALLBACK(OnButtonPress), this);
    signals_.Connect(button(), "clicked",
                     G_CALLBACK(OnClicked), this);
    signals_.Connect(button(), "drag-begin",
                     G_CALLBACK(OnDragBegin), this);
    signals_.ConnectAfter(widget(), "expose-event",
                     G_CALLBACK(OnExposeEvent), this);
    if (toolbar_->browser()->window()) {
      // If the window exists already, then the browser action button has been
      // recreated after the window was created, for example when the extension
      // is reloaded.
      ConnectBrowserActionPopupAccelerator();
    } else {
      // Window doesn't exist yet, wait for it.
      signals_.Connect(toolbar->widget(), "realize",
                       G_CALLBACK(OnRealize), this);
    }

    registrar_.Add(
        this, chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
        content::Source<ExtensionAction>(browser_action()));
    registrar_.Add(
        this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
        content::Source<Profile>(
            toolbar->browser()->profile()->GetOriginalProfile()));
    registrar_.Add(
        this, chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED,
        content::Source<Profile>(
        toolbar->browser()->profile()->GetOriginalProfile()));
    registrar_.Add(
        this, chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
        content::Source<Profile>(
        toolbar->browser()->profile()->GetOriginalProfile()));
  }

  virtual ~BrowserActionButton() {
    DisconnectBrowserActionPopupAccelerator();

    alignment_.Destroy();
  }

  GtkWidget* button() { return button_->widget(); }

  GtkWidget* widget() { return alignment_.get(); }

  const Extension* extension() { return extension_; }

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
     case chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED:
      UpdateState();
      break;
     case chrome::NOTIFICATION_EXTENSION_UNLOADED:
     case chrome::NOTIFICATION_WINDOW_CLOSED:
      DisconnectBrowserActionPopupAccelerator();
      break;
     case chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED:
     case chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED: {
      std::pair<const std::string, const std::string>* payload =
          content::Details<std::pair<const std::string, const std::string> >(
              details).ptr();
      if (extension_->id() == payload->first &&
          payload->second ==
              extension_manifest_values::kBrowserActionCommandEvent) {
        if (type == chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED)
          ConnectBrowserActionPopupAccelerator();
        else
          DisconnectBrowserActionPopupAccelerator();
      }
      break;
     }
     default:
      NOTREACHED();
      break;
    }
  }

  // ExtensionActionIconFactory::Observer implementation.
  virtual void OnIconUpdated() OVERRIDE {
    UpdateState();
  }

  // Updates the button based on the latest state from the associated
  // browser action.
  void UpdateState() {
    int tab_id = toolbar_->GetCurrentTabId();
    if (tab_id < 0)
      return;

    std::string tooltip = browser_action()->GetTitle(tab_id);
    if (tooltip.empty())
      gtk_widget_set_has_tooltip(button(), FALSE);
    else
      gtk_widget_set_tooltip_text(button(), tooltip.c_str());

    enabled_ = browser_action()->GetIsVisible(tab_id);
    if (!enabled_)
      button_->SetPaintOverride(GTK_STATE_INSENSITIVE);
    else
      button_->UnsetPaintOverride();

    gfx::Image image = icon_factory_.GetIcon(tab_id);
    if (!image.IsEmpty()) {
      if (enabled_) {
        SetImage(image);
      } else {
        SetImage(gfx::Image(gfx::ImageSkiaOperations::CreateTransparentImage(
            image.AsImageSkia(), .25)));
      }
    }

    gtk_widget_queue_draw(button());
  }

  gfx::Image GetIcon() {
    return icon_factory_.GetIcon(toolbar_->GetCurrentTabId());
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
  // Activate the browser action.
  void Activate(GtkWidget* widget) {
    ExtensionToolbarModel* model = toolbar_->model();
    const Extension* extension = extension_;
    Browser* browser = toolbar_->browser();
    GURL popup_url;

    switch (model->ExecuteBrowserAction(extension, browser, &popup_url)) {
      case ExtensionToolbarModel::ACTION_NONE:
        break;
      case ExtensionToolbarModel::ACTION_SHOW_POPUP:
        ExtensionPopupGtk::Show(popup_url, browser, widget,
                                ExtensionPopupGtk::SHOW);
        break;
    }
  }

  // MenuGtk::Delegate implementation.
  virtual void StoppedShowing() OVERRIDE {
    if (enabled_)
      button_->UnsetPaintOverride();
    else
      button_->SetPaintOverride(GTK_STATE_INSENSITIVE);

    // If the context menu was showing for the overflow menu, re-assert the
    // grab that was shadowed.
    if (toolbar_->overflow_menu_.get())
      gtk_util::GrabAllInput(toolbar_->overflow_menu_->widget());
  }

  virtual void CommandWillBeExecuted() OVERRIDE {
    // If the context menu was showing for the overflow menu, and a command
    // is executed, then stop showing the overflow menu.
    if (toolbar_->overflow_menu_.get())
      toolbar_->overflow_menu_->Cancel();
  }

  // ExtensionContextMenuModel::PopupDelegate implementation.
  virtual void InspectPopup(ExtensionAction* action) OVERRIDE {
    GURL popup_url = action->GetPopupUrl(toolbar_->GetCurrentTabId());
    ExtensionPopupGtk::Show(popup_url, toolbar_->browser(), widget(),
                            ExtensionPopupGtk::SHOW_AND_INSPECT);
  }

  void SetImage(const gfx::Image& image) {
    if (!image_) {
      image_ = gtk_image_new_from_pixbuf(image.ToGdkPixbuf());
      gtk_button_set_image(GTK_BUTTON(button()), image_);
    } else {
      gtk_image_set_from_pixbuf(GTK_IMAGE(image_), image.ToGdkPixbuf());
    }
  }

  static gboolean OnButtonPress(GtkWidget* widget,
                                GdkEventButton* event,
                                BrowserActionButton* button) {
    if (event->button != 3)
      return FALSE;

    MenuGtk* menu = button->GetContextMenu();
    if (!menu)
      return FALSE;

    button->button_->SetPaintOverride(GTK_STATE_ACTIVE);
    menu->PopupForWidget(widget, event->button, event->time);

    return TRUE;
  }

  static void OnClicked(GtkWidget* widget, BrowserActionButton* button) {
    if (button->enabled_)
      button->Activate(widget);
  }

  static gboolean OnExposeEvent(GtkWidget* widget,
                                GdkEventExpose* event,
                                BrowserActionButton* button) {
    int tab_id = button->toolbar_->GetCurrentTabId();
    if (tab_id < 0)
      return FALSE;

    ExtensionAction* action = button->browser_action();
    if (action->GetBadgeText(tab_id).empty())
      return FALSE;

    gfx::CanvasSkiaPaint canvas(event, false);
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    action->PaintBadge(&canvas, gfx::Rect(allocation), tab_id);
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

  // The accelerator handler for when the shortcuts to open the popup is struck.
  static gboolean OnGtkAccelerator(GtkAccelGroup* accel_group,
                                   GObject* acceleratable,
                                   guint keyval,
                                   GdkModifierType modifier,
                                   BrowserActionButton* button) {
    // Open the popup for this extension.
    GtkWidget* anchor = button->widget();
    // The anchor might be in the overflow menu. Then we point to the chevron.
    if (!gtk_widget_get_visible(anchor))
      anchor = button->toolbar_->chevron();
    button->Activate(anchor);
    return TRUE;
  }

  // The handler for when the browser action is realized. |user_data| contains a
  // pointer to the BrowserAction shown.
  static void OnRealize(GtkWidget* widget, void* user_data) {
    BrowserActionButton* button = static_cast<BrowserActionButton*>(user_data);
    button->ConnectBrowserActionPopupAccelerator();
  }

  // Connect the accelerator for the browser action popup.
  void ConnectBrowserActionPopupAccelerator() {
    extensions::CommandService* command_service =
        extensions::CommandService::Get(toolbar_->browser()->profile());
    extensions::Command command;
    if (command_service->GetBrowserActionCommand(extension_->id(),
        extensions::CommandService::ACTIVE_ONLY,
        &command,
        NULL)) {
      // Found the browser action shortcut command, register it.
      keybinding_ = command.accelerator();

      gfx::NativeWindow window =
          toolbar_->browser()->window()->GetNativeWindow();
      accel_group_ = gtk_accel_group_new();
      gtk_window_add_accel_group(window, accel_group_);

      gtk_accel_group_connect(
          accel_group_,
          ui::GetGdkKeyCodeForAccelerator(keybinding_),
          ui::GetGdkModifierForAccelerator(keybinding_),
          GtkAccelFlags(0),
          g_cclosure_new(G_CALLBACK(OnGtkAccelerator), this, NULL));

      // Since we've added an accelerator, we'll need to unregister it before
      // the window is closed, so we listen for the window being closed.
      registrar_.Add(this,
                     chrome::NOTIFICATION_WINDOW_CLOSED,
                     content::Source<GtkWindow>(window));
    }
  }

  // Disconnect the accelerator for the browser action popup and delete clean up
  // the accelerator group registration.
  void DisconnectBrowserActionPopupAccelerator() {
    if (accel_group_) {
      gfx::NativeWindow window =
          toolbar_->browser()->window()->GetNativeWindow();
      gtk_accel_group_disconnect_key(
          accel_group_,
          ui::GetGdkKeyCodeForAccelerator(keybinding_),
          GetGdkModifierForAccelerator(keybinding_));
      gtk_window_remove_accel_group(window, accel_group_);
      g_object_unref(accel_group_);
      accel_group_ = NULL;
      keybinding_ = ui::Accelerator();

      // We've removed the accelerator, so no need to listen to this anymore.
      registrar_.Remove(this,
                        chrome::NOTIFICATION_WINDOW_CLOSED,
                        content::Source<GtkWindow>(window));
    }
  }

  ExtensionAction* browser_action() const {
    return ExtensionActionManager::Get(toolbar_->browser()->profile())->
        GetBrowserAction(*extension_);
  }

  // The toolbar containing this button.
  BrowserActionsToolbarGtk* toolbar_;

  // The extension that contains this browser action.
  const Extension* extension_;

  // The button for this browser action.
  scoped_ptr<CustomDrawButton> button_;

  // Whether the browser action is enabled (equivalent to whether a page action
  // is visible).
  bool enabled_;

  // The top level widget (parent of |button_|).
  ui::OwnedWidgetGtk alignment_;

  // The one image subwidget in |button_|. We keep this out so we don't alter
  // the widget hierarchy while changing the button image because changing the
  // GTK widget hierarchy invalidates all tooltips and several popular
  // extensions change browser action icon in a loop.
  GtkWidget* image_;

  // The object that will be used to get the browser action icon for us.
  // It may load the icon asynchronously (in which case the initial icon
  // returned by the factory will be transparent), so we have to observe it for
  // updates to the icon.
  ExtensionActionIconFactory icon_factory_;

  // Same as |default_icon_|, but stored as SkBitmap.
  SkBitmap default_skbitmap_;

  ui::GtkSignalRegistrar signals_;
  content::NotificationRegistrar registrar_;

  // The accelerator group used to handle accelerators, owned by this object.
  GtkAccelGroup* accel_group_;

  // The keybinding accelerator registered to show the browser action popup.
  ui::Accelerator keybinding_;

  // The context menu view and model for this extension action.
  scoped_ptr<MenuGtk> context_menu_;
  scoped_refptr<ExtensionContextMenuModel> context_menu_model_;

  friend class BrowserActionsToolbarGtk;
};

// BrowserActionsToolbarGtk ----------------------------------------------------

BrowserActionsToolbarGtk::BrowserActionsToolbarGtk(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      theme_service_(GtkThemeService::GetFrom(browser->profile())),
      model_(NULL),
      hbox_(gtk_hbox_new(FALSE, 0)),
      button_hbox_(gtk_chrome_shrinkable_hbox_new(TRUE, FALSE, kButtonPadding)),
      drag_button_(NULL),
      drop_index_(-1),
      resize_animation_(this),
      desired_width_(0),
      start_width_(0),
      weak_factory_(this) {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!extension_service)
    return;

  overflow_button_.reset(new CustomDrawButton(
      theme_service_,
      IDR_BROWSER_ACTIONS_OVERFLOW,
      IDR_BROWSER_ACTIONS_OVERFLOW_P,
      IDR_BROWSER_ACTIONS_OVERFLOW_H,
      0,
      gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE)));

  GtkWidget* gripper = gtk_button_new();
  gtk_widget_set_size_request(gripper, kResizeGripperWidth, -1);
  gtk_widget_set_can_focus(gripper, FALSE);

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
                 chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  theme_service_->InitThemesFor(this);
}

BrowserActionsToolbarGtk::~BrowserActionsToolbarGtk() {
  if (model_)
    model_->RemoveObserver(this);
  button_hbox_.Destroy();
  hbox_.Destroy();
}

int BrowserActionsToolbarGtk::GetCurrentTabId() const {
  content::WebContents* active_tab =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!active_tab)
    return -1;

  return SessionTabHelper::FromWebContents(active_tab)->session_id().id();
}

void BrowserActionsToolbarGtk::Update() {
  for (ExtensionButtonMap::iterator iter = extension_button_map_.begin();
       iter != extension_button_map_.end(); ++iter) {
    iter->second->UpdateState();
  }
}

void BrowserActionsToolbarGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(chrome::NOTIFICATION_BROWSER_THEME_CHANGED == type);
  gtk_widget_set_visible(separator_, theme_service_->UsingNativeTheme());
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
  const extensions::ExtensionList& toolbar_items = model_->toolbar_items();
  for (extensions::ExtensionList::const_iterator iter = toolbar_items.begin();
       iter != toolbar_items.end(); ++iter) {
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
      new BrowserActionButton(this, extension, theme_service_));
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
  gtk_widget_set_visible(widget(), button_count() != 0);
}

bool BrowserActionsToolbarGtk::ShouldDisplayBrowserAction(
    const Extension* extension) {
  // Only display incognito-enabled extensions while in incognito mode.
  return (!profile_->IsOffTheRecord() ||
          extensions::ExtensionSystem::Get(profile_)->extension_service()->
              IsIncognitoEnabled(extension->id()));
}

void BrowserActionsToolbarGtk::HidePopup() {
  ExtensionPopupGtk* popup = ExtensionPopupGtk::get_current_extension_popup();
  if (popup)
    popup->DestroyPopup();
}

void BrowserActionsToolbarGtk::AnimateToShowNIcons(int count) {
  desired_width_ = WidthForIconCount(count);

  GtkAllocation allocation;
  gtk_widget_get_allocation(button_hbox_.get(), &allocation);
  start_width_ = allocation.width;

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
  if (!gtk_widget_get_visible(overflow_area_)) {
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

  if (!gtk_widget_get_visible(overflow_area_)) {
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
  const Extension* extension = model_->toolbar_items()[command_id];
  return ExtensionActionManager::Get(profile_)->
      GetBrowserAction(*extension)->GetIsVisible(GetCurrentTabId());
}

bool BrowserActionsToolbarGtk::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void BrowserActionsToolbarGtk::ExecuteCommand(int command_id, int event_flags) {
  const Extension* extension = model_->toolbar_items()[command_id];
  GURL popup_url;

  switch (model_->ExecuteBrowserAction(extension, browser(), &popup_url)) {
    case ExtensionToolbarModel::ACTION_NONE:
      break;
    case ExtensionToolbarModel::ACTION_SHOW_POPUP:
      ExtensionPopupGtk::Show(popup_url, browser(), chevron(),
                              ExtensionPopupGtk::SHOW);
      break;
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
    if (!gtk_widget_get_visible(overflow_area_)) {
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

  if (base::i18n::IsRTL()) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    x = allocation.width - x;
  }

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
  if (!gtk_widget_is_toplevel(toplevel))
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
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserActionsToolbarGtk::HidePopup,
                 weak_factory_.GetWeakPtr()));
}

gboolean BrowserActionsToolbarGtk::OnGripperMotionNotify(
    GtkWidget* widget, GdkEventMotion* event) {
  if (!(event->state & GDK_BUTTON1_MASK))
    return FALSE;

  // Calculate how much the user dragged the gripper and subtract that off the
  // button container's width.
  int distance_dragged;
  if (base::i18n::IsRTL()) {
    distance_dragged = -event->x;
  } else {
    GtkAllocation widget_allocation;
    gtk_widget_get_allocation(widget, &widget_allocation);
    distance_dragged = event->x - widget_allocation.width;
  }

  GtkAllocation button_hbox_allocation;
  gtk_widget_get_allocation(button_hbox_.get(), &button_hbox_allocation);
  gint new_width = button_hbox_allocation.width - distance_dragged;
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
  gdk_window_set_cursor(gtk_widget_get_window(gripper),
                        gfx::GetCursor(GDK_SB_H_DOUBLE_ARROW));
  return FALSE;
}

gboolean BrowserActionsToolbarGtk::OnGripperLeaveNotify(
    GtkWidget* gripper, GdkEventCrossing* event) {
  if (!(event->state & GDK_BUTTON1_MASK))
    gdk_window_set_cursor(gtk_widget_get_window(gripper), NULL);
  return FALSE;
}

gboolean BrowserActionsToolbarGtk::OnGripperButtonRelease(
    GtkWidget* gripper, GdkEventButton* event) {
  GtkAllocation allocation;
  gtk_widget_get_allocation(gripper, &allocation);
  gfx::Rect gripper_rect(0, 0, allocation.width, allocation.height);

  gfx::Point release_point(event->x, event->y);
  if (!gripper_rect.Contains(release_point))
    gdk_window_set_cursor(gtk_widget_get_window(gripper), NULL);

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

    const Extension* extension = model_->toolbar_items()[model_index];
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

  const Extension* extension = model_->toolbar_items()[item_index];
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
