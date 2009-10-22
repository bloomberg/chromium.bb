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
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

// The size of each button on the toolbar.
static const int kButtonSize = 29;

class BrowserActionButton : public NotificationObserver,
                            public ImageLoadingTracker::Observer {
 public:
  BrowserActionButton(Browser* browser, Extension* extension)
      : browser_(browser),
        extension_(extension),
        button_(gtk_chrome_button_new()) {
    DCHECK(extension_->browser_action());

    gtk_widget_set_size_request(button_.get(), kButtonSize, kButtonSize);

    browser_action_icons_.resize(
        extension->browser_action()->icon_paths().size(), NULL);
    tracker_ = new ImageLoadingTracker(this, browser_action_icons_.size());
    for (size_t i = 0; i < extension->browser_action()->icon_paths().size();
         ++i) {
      tracker_->PostLoadImageTask(
          extension->GetResource(extension->browser_action()->icon_paths()[i]),
          gfx::Size(Extension::kBrowserActionIconMaxSize,
                    Extension::kBrowserActionIconMaxSize));
    }

    OnStateUpdated();

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
    for (size_t i = 0; i < browser_action_icons_.size(); ++i) {
      if (browser_action_icons_[i])
        g_object_unref(browser_action_icons_[i]);
    }

    button_.Destroy();
    tracker_->StopTrackingImageLoad();
  }

  GtkWidget* widget() { return button_.get(); }

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type == NotificationType::EXTENSION_BROWSER_ACTION_UPDATED)
      OnStateUpdated();
    else if (type == NotificationType::BROWSER_THEME_CHANGED)
      OnThemeChanged();
    else
      NOTREACHED();
  }

  // ImageLoadingTracker::Observer implementation.
  void OnImageLoaded(SkBitmap* image, size_t index) {
    SkBitmap empty;
    SkBitmap* bitmap = image ? image : &empty;
    browser_action_icons_[index] = gfx::GdkPixbufFromSkBitmap(bitmap);
    OnStateUpdated();
  }

 private:
  // Called when the tooltip has changed or an image has loaded.
  void OnStateUpdated() {
    gtk_widget_set_tooltip_text(button_.get(),
        extension_->browser_action_state()->title().c_str());

    if (browser_action_icons_.empty())
      return;

    GdkPixbuf* image =
        browser_action_icons_[extension_->browser_action_state()->icon_index()];
    if (image) {
      gtk_button_set_image(GTK_BUTTON(button_.get()),
                           gtk_image_new_from_pixbuf(image));
    }
  }

  void OnThemeChanged() {
    gtk_chrome_button_set_use_gtk_rendering(GTK_CHROME_BUTTON(button_.get()),
        GtkThemeProvider::GetFrom(browser_->profile())->UseGtkTheme());
  }

  static void OnButtonClicked(GtkWidget* widget, BrowserActionButton* action) {
    if (action->extension_->browser_action()->is_popup()) {
      ExtensionPopupGtk::Show(action->extension_->browser_action()->popup_url(),
                              action->browser_, gfx::Rect(widget->allocation));

    } else {
      ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
          action->browser_->profile(), action->extension_->id(),
          action->browser_);
    }
  }

  static gboolean OnExposeEvent(GtkWidget* widget,
                                GdkEventExpose* event,
                                BrowserActionButton* action) {
    if (action->extension_->browser_action_state()->badge_text().empty())
      return FALSE;

    gfx::CanvasPaint canvas(event, false);
    gfx::Rect bounding_rect(widget->allocation);
    action->extension_->browser_action_state()->PaintBadge(&canvas,
                                                           bounding_rect);
    return FALSE;
  }

  // The Browser that executes a command when the button is pressed.
  Browser* browser_;

  // The extension that contains this browser action.
  Extension* extension_;

  // The gtk widget for this browser action.
  OwnedWidgetGtk button_;

  // Loads the button's icons for us on the file thread.
  ImageLoadingTracker* tracker_;

  // Icons for all the different states the button can be in. These will be NULL
  // while they are loading.
  std::vector<GdkPixbuf*> browser_action_icons_;

  NotificationRegistrar registrar_;
};

BrowserActionsToolbarGtk::BrowserActionsToolbarGtk(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      hbox_(gtk_hbox_new(0, FALSE)) {
  ExtensionsService* extension_service = profile_->GetExtensionsService();
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<ExtensionsService>(extension_service));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<ExtensionsService>(extension_service));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                 Source<ExtensionsService>(extension_service));

  CreateAllButtons();
}

BrowserActionsToolbarGtk::~BrowserActionsToolbarGtk() {
  hbox_.Destroy();
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

  // Get all browser actions, including those with popups.
  std::vector<ExtensionAction*> browser_actions =
      extension_service->GetBrowserActions(true);

  for (size_t i = 0; i < browser_actions.size(); ++i) {
    Extension* extension = extension_service->GetExtensionById(
        browser_actions[i]->extension_id());
    CreateButtonForExtension(extension);
  }
}

void BrowserActionsToolbarGtk::CreateButtonForExtension(Extension* extension) {
  // Only show extensions with browser actions and that have an icon.
  if (!extension->browser_action() ||
      extension->browser_action()->icon_paths().empty()) {
    return;
  }

  RemoveButtonForExtension(extension);
  linked_ptr<BrowserActionButton> button(
      new BrowserActionButton(browser_, extension));
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
