// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_actions_toolbar_gtk.h"

#include <gtk/gtk.h>
#include <vector>

#include "app/gfx/canvas_paint.h"
#include "app/gfx/gtk_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/gtk/extension_popup_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

// The size of each button on the toolbar.
static const int kButtonSize = 29;

// The padding between browser action buttons. Visually, the actual number of
// "empty" (non-drawing) pixels is this value + 2 when adjacent browser icons
// use their maximum allowed size.
static const int kBrowserActionButtonPadding = 3;

class BrowserActionButton : public NotificationObserver,
                            public ImageLoadingTracker::Observer {
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

    // We need to hook up extension popups here. http://crbug.com/23897
    g_signal_connect(button_.get(), "clicked",
                     G_CALLBACK(OnButtonClicked), this);
    g_signal_connect_after(button_.get(), "expose-event",
                           G_CALLBACK(OnExposeEvent), this);

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

  static void OnButtonClicked(GtkWidget* widget, BrowserActionButton* action) {
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
};

BrowserActionsToolbarGtk::BrowserActionsToolbarGtk(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      hbox_(gtk_hbox_new(FALSE, kBrowserActionButtonPadding)) {
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                 Source<Profile>(profile_));

  CreateAllButtons();
}

BrowserActionsToolbarGtk::~BrowserActionsToolbarGtk() {
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
  Extension* extension = Details<Extension>(details).ptr();

  if (type == NotificationType::EXTENSION_LOADED) {
    CreateButtonForExtension(extension);
  } else if (type == NotificationType::EXTENSION_UNLOADED ||
             type == NotificationType::EXTENSION_UNLOADED_DISABLED) {
    RemoveButtonForExtension(extension);
  } else {
    NOTREACHED() << "Received unexpected notification";
  }
}

void BrowserActionsToolbarGtk::CreateAllButtons() {
  ExtensionsService* extension_service = profile_->GetExtensionsService();
  if (!extension_service)  // The |extension_service| can be NULL in Incognito.
    return;

  for (size_t i = 0; i < extension_service->extensions()->size(); ++i) {
    Extension* extension = extension_service->GetExtensionById(
        extension_service->extensions()->at(i)->id(), false);
    CreateButtonForExtension(extension);
  }
}

void BrowserActionsToolbarGtk::CreateButtonForExtension(Extension* extension) {
  // Only show extensions with browser actions.
  if (!extension->browser_action())
    return;

  RemoveButtonForExtension(extension);
  linked_ptr<BrowserActionButton> button(
      new BrowserActionButton(this, extension));
  gtk_box_pack_end(GTK_BOX(hbox_.get()), button->widget(), FALSE, FALSE, 0);
  gtk_widget_show(button->widget());
  extension_button_map_[extension->id()] = button;

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
