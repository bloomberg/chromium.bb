// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_actions_toolbar_gtk.h"

#include <vector>

#include "app/gfx/canvas_paint.h"
#include "app/gfx/gtk_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_action_context_menu_model.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/gtk/extension_popup_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

namespace {

// The size of each button on the toolbar.
const int kButtonSize = 29;

// The padding between browser action buttons. Visually, the actual number of
// "empty" (non-drawing) pixels is this value + 2 when adjacent browser icons
// use their maximum allowed size.
const int kButtonPadding = 3;

const char* kDragTarget = "application/x-chrome-browseraction";

GtkTargetEntry GetDragTargetEntry() {
  static std::string drag_target_string(kDragTarget);
  GtkTargetEntry drag_target;
  drag_target.target = const_cast<char*>(drag_target_string.c_str());
  drag_target.flags = GTK_TARGET_SAME_APP;
  drag_target.info = 0;
  return drag_target;
}

}  // namespace

class BrowserActionButton : public NotificationObserver,
                            public ImageLoadingTracker::Observer,
                            public MenuGtk::Delegate {
 public:
  BrowserActionButton(BrowserActionsToolbarGtk* toolbar,
                      Extension* extension)
      : toolbar_(toolbar),
        extension_(extension),
        button_(gtk_chrome_button_new()),
        tracker_(NULL),
        tab_specific_icon_(NULL),
        default_icon_(NULL) {
    DCHECK(extension_->browser_action());

    gtk_widget_set_size_request(button_.get(), kButtonSize, kButtonSize);

    UpdateState();

    // The Browser Action API does not allow the default icon path to be
    // changed at runtime, so we can load this now and cache it.
    std::string path = extension_->browser_action()->default_icon_path();
    if (!path.empty()) {
      tracker_ = new ImageLoadingTracker(this, 1);
      tracker_->PostLoadImageTask(extension_->GetResource(path),
          gfx::Size(Extension::kBrowserActionIconMaxSize,
                    Extension::kBrowserActionIconMaxSize));
    }

    g_signal_connect(button_.get(), "button-press-event",
                     G_CALLBACK(OnButtonPress), this);
    g_signal_connect(button_.get(), "clicked",
                     G_CALLBACK(OnClicked), this);
    g_signal_connect_after(button_.get(), "expose-event",
                           G_CALLBACK(OnExposeEvent), this);
    g_signal_connect(button_.get(), "drag-begin",
                     G_CALLBACK(&OnDragBegin), this);

    registrar_.Add(this, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                   Source<ExtensionAction>(extension->browser_action()));
    registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                   NotificationService::AllSources());

    OnThemeChanged();
  }

  ~BrowserActionButton() {
    if (tab_specific_icon_)
      g_object_unref(tab_specific_icon_);

    if (default_icon_)
      g_object_unref(default_icon_);

    button_.Destroy();

    if (tracker_)
      tracker_->StopTrackingImageLoad();
  }

  GtkWidget* widget() { return button_.get(); }

  Extension* extension() { return extension_; }

  // NotificationObserver implementation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type == NotificationType::EXTENSION_BROWSER_ACTION_UPDATED)
      UpdateState();
    else if (type == NotificationType::BROWSER_THEME_CHANGED)
      OnThemeChanged();
    else
      NOTREACHED();
  }

  // ImageLoadingTracker::Observer implementation.
  void OnImageLoaded(SkBitmap* image, size_t index) {
    if (image)
      default_icon_ = gfx::GdkPixbufFromSkBitmap(image);
    tracker_ = NULL;  // The tracker object will delete itself when we return.
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
      gtk_widget_set_has_tooltip(button_.get(), FALSE);
    else
      gtk_widget_set_tooltip_text(button_.get(), tooltip.c_str());

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
    gtk_widget_queue_draw(button_.get());
  }

 private:
  void SetImage(GdkPixbuf* image) {
    gtk_button_set_image(GTK_BUTTON(button_.get()),
        gtk_image_new_from_pixbuf(image));
  }

  void OnThemeChanged() {
    gtk_chrome_button_set_use_gtk_rendering(GTK_CHROME_BUTTON(button_.get()),
        GtkThemeProvider::GetFrom(
            toolbar_->browser()->profile())->UseGtkTheme());
  }

  static gboolean OnButtonPress(GtkWidget* widget,
                                GdkEvent* event,
                                BrowserActionButton* action) {
    if (event->button.button != 3)
      return FALSE;

    if (!action->context_menu_model_.get()) {
      action->context_menu_model_.reset(
          new ExtensionActionContextMenuModel(action->extension_));
    }

    action->context_menu_.reset(
        new MenuGtk(action, action->context_menu_model_.get()));

    action->context_menu_->Popup(widget, event);
    return TRUE;
  }

  static void OnClicked(GtkWidget* widget, BrowserActionButton* action) {
    if (action->extension_->browser_action()->has_popup()) {
      ExtensionPopupGtk::Show(
          action->extension_->browser_action()->popup_url(),
          action->toolbar_->browser(),
          gtk_util::GetWidgetRectRelativeToToplevel(widget));
    } else {
      ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
          action->toolbar_->browser()->profile(), action->extension_->id(),
          action->toolbar_->browser());
    }
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

    gfx::CanvasPaint canvas(event, false);
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
  Extension* extension_;

  // The gtk widget for this browser action.
  OwnedWidgetGtk button_;

  // Loads the button's icons for us on the file thread.
  ImageLoadingTracker* tracker_;

  // If we are displaying a tab-specific icon, it will be here.
  GdkPixbuf* tab_specific_icon_;

  // If the browser action has a default icon, it will be here.
  GdkPixbuf* default_icon_;

  NotificationRegistrar registrar_;

  // The context menu view and model for this extension action.
  scoped_ptr<MenuGtk> context_menu_;
  scoped_ptr<ExtensionActionContextMenuModel> context_menu_model_;

  friend class BrowserActionsToolbarGtk;
};

BrowserActionsToolbarGtk::BrowserActionsToolbarGtk(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      model_(NULL),
      hbox_(gtk_hbox_new(FALSE, kButtonPadding)),
      drag_button_(NULL),
      drop_index_(-1) {
  ExtensionsService* extension_service = profile_->GetExtensionsService();
  // The |extension_service| can be NULL in Incognito.
  if (!extension_service)
    return;

  model_ = extension_service->toolbar_model();
  model_->AddObserver(this);
  SetupDrags();
  CreateAllButtons();
}

BrowserActionsToolbarGtk::~BrowserActionsToolbarGtk() {
  if (model_)
    model_->RemoveObserver(this);
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

void BrowserActionsToolbarGtk::SetupDrags() {
  GtkTargetEntry drag_target = GetDragTargetEntry();
  gtk_drag_dest_set(widget(), GTK_DEST_DEFAULT_DROP, &drag_target, 1,
                    GDK_ACTION_MOVE);

  g_signal_connect(widget(), "drag-motion",
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

void BrowserActionsToolbarGtk::CreateButtonForExtension(Extension* extension,
                                                        int index) {
  RemoveButtonForExtension(extension);
  linked_ptr<BrowserActionButton> button(
      new BrowserActionButton(this, extension));
  gtk_box_pack_start(GTK_BOX(hbox_.get()), button->widget(), FALSE, FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(hbox_.get()), button->widget(), index);
  gtk_widget_show(button->widget());
  extension_button_map_[extension->id()] = button;

  GtkTargetEntry drag_target = GetDragTargetEntry();
  gtk_drag_source_set(button->widget(), GDK_BUTTON1_MASK, &drag_target, 1,
                      GDK_ACTION_MOVE);
  // We ignore whether the drag was a "success" or "failure" in Gtk's opinion.
  g_signal_connect(button->widget(), "drag-end",
                   G_CALLBACK(&OnDragEndThunk), this);
  g_signal_connect(button->widget(), "drag-failed",
                   G_CALLBACK(&OnDragFailedThunk), this);

  UpdateVisibility();
}

void BrowserActionsToolbarGtk::RemoveButtonForExtension(Extension* extension) {
  if (extension_button_map_.erase(extension->id()))
    UpdateVisibility();
}

void BrowserActionsToolbarGtk::UpdateVisibility() {
  if (button_count() == 0)
    gtk_widget_hide(widget());
  else
    gtk_widget_show(widget());
}

void BrowserActionsToolbarGtk::BrowserActionAdded(Extension* extension,
                                                  int index) {
  CreateButtonForExtension(extension, index);
}

void BrowserActionsToolbarGtk::BrowserActionRemoved(Extension* extension) {
  if (drag_button_ != NULL) {
    // Break the current drag.
    gtk_grab_remove(widget());
  }

  RemoveButtonForExtension(extension);
}

void BrowserActionsToolbarGtk::BrowserActionMoved(Extension* extension,
                                                  int index) {
  // We initiated this move action, and have already moved the button.
  if (drag_button_ != NULL)
    return;

  BrowserActionButton* button = extension_button_map_[extension->id()].get();
  if (!button) {
    NOTREACHED();
    return;
  }

  gtk_box_reorder_child(GTK_BOX(hbox_.get()), button->widget(), index);
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

gboolean BrowserActionsToolbarGtk::OnDragMotion(GtkWidget* widget,
                                                GdkDragContext* drag_context,
                                                gint x, gint y, guint time) {
  // Only handle drags we initiated.
  if (!drag_button_)
    return FALSE;

  drop_index_ = x < kButtonSize ? 0 : x / (kButtonSize + kButtonPadding);
  // We will go ahead and reorder the child in order to provide visual feedback
  // to the user. We don't inform the model that it has moved until the drag
  // ends.
  gtk_box_reorder_child(GTK_BOX(hbox_.get()), drag_button_->widget(),
                        drop_index_);

  gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);
  return TRUE;
}

void BrowserActionsToolbarGtk::OnDragEnd(GtkWidget* button,
                                         GdkDragContext* drag_context) {
  if (drop_index_ != -1)
    model_->MoveBrowserAction(drag_button_->extension(), drop_index_);

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
