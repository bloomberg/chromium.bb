// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_toolbar_gtk.h"

#include <gdk/gdkkeysyms.h>
#include <X11/XF86keysym.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/base_paths_linux.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/encoding_menu_controller.h"
#include "chrome/browser/gtk/back_forward_button_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/go_button_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_dnd_util.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/gtk/toolbar_star_toggle_gtk.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Height of the toolbar in pixels.
const int kToolbarHeight = 37;

// Height of the toolbar in pixels when we only show the location bar.
const int kToolbarHeightLocationBarOnly = kToolbarHeight - 10;

// Interior spacing between toolbar widgets.
const int kToolbarWidgetSpacing = 4;

// The amount of space between the bottom of the star and the top of the
// Omnibox results popup window.  We want a two pixel space between the bottom
// and the results, but have some extra space below the buttons already.
const int kPopupTopMargin = 0;

// Space between the edge of the star/go button and the popup frame.  We want
// to leave 1 pixel on both side here so that the borders line up.
const int kPopupLeftRightMargin = 1;

// The color used as the base[] color of the location entry during a secure
// connection.
const GdkColor kSecureColor = GDK_COLOR_RGB(255, 245, 195);

}  // namespace

// BrowserToolbarGtk, public ---------------------------------------------------

BrowserToolbarGtk::BrowserToolbarGtk(Browser* browser, BrowserWindowGtk* window)
    : toolbar_(NULL),
      location_bar_(new LocationBarViewGtk(browser->command_updater(),
                                           browser->toolbar_model(),
                                           this)),
      model_(browser->toolbar_model()),
      browser_(browser),
      window_(window),
      profile_(NULL) {
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_RELOAD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_STAR, this);

  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

BrowserToolbarGtk::~BrowserToolbarGtk() {
  offscreen_entry_.Destroy();

  // When we created our MenuGtk objects, we pass them a pointer to our accel
  // group. Make sure to tear them down before |accel_group_|.
  page_menu_.reset();
  app_menu_.reset();
  page_menu_button_.Destroy();
  app_menu_button_.Destroy();
  g_object_unref(accel_group_);
}

// Construct an "encodings" menu based on profile settings.
static MenuGtk* BuildEncodingsMenu(Profile* profile,
                                   MenuGtk::Delegate* delegate) {
  EncodingMenuController controller;
  EncodingMenuController::EncodingMenuItemList items;
  controller.GetEncodingMenuItems(profile, &items);

  MenuGtk* menu = new MenuGtk(delegate, false);
  GSList* radio_group = NULL;
  for (EncodingMenuController::EncodingMenuItemList::const_iterator i =
           items.begin();
       i != items.end(); ++i) {
    if (i == items.begin()) {
      menu->AppendCheckMenuItemWithLabel(i->first, WideToUTF8(i->second));
    } else if (i->first == 0) {
      menu->AppendSeparator();
    } else {
      GtkWidget* item =
          gtk_radio_menu_item_new_with_label(radio_group,
                                             WideToUTF8(i->second).c_str());
      radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      menu->AppendMenuItem(i->first, item);
    }
  }
  return menu;
}

void BrowserToolbarGtk::Init(Profile* profile,
                             GtkWindow* top_level_window) {
  // Make sure to tell the location bar the profile before calling its Init.
  SetProfile(profile);

  theme_provider_ = GtkThemeProvider::GetFrom(profile);
  offscreen_entry_.Own(gtk_entry_new());

  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  event_box_ = gtk_event_box_new();
  // Make the event box transparent so themes can use transparent toolbar
  // backgrounds.
  if (!theme_provider_->UseGtkTheme())
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_), FALSE);

  toolbar_ = gtk_hbox_new(FALSE, kToolbarWidgetSpacing);
  gtk_container_add(GTK_CONTAINER(event_box_), toolbar_);
  gtk_container_set_border_width(GTK_CONTAINER(toolbar_),
      ShouldOnlyShowLocation() ? 0 : 4);
  // Force the height of the toolbar so we get the right amount of padding
  // above and below the location bar. -1 for width means "let GTK do its
  // normal sizing".
  gtk_widget_set_size_request(toolbar_, -1, ShouldOnlyShowLocation() ?
      kToolbarHeightLocationBarOnly : kToolbarHeight);
  g_signal_connect(toolbar_, "expose-event",
                   G_CALLBACK(&OnToolbarExpose), this);

  // A GtkAccelGroup is not InitiallyUnowned, meaning we get a real reference
  // count starting at one.  We don't want the lifetime to be managed by the
  // top level window, since the lifetime should be tied to the C++ object.
  // When we add the accelerator group, the window will take a reference, but
  // we still hold on to the original, and thus own a reference to the group.
  accel_group_ = gtk_accel_group_new();
  gtk_window_add_accel_group(top_level_window, accel_group_);

  // Group back and forward into an hbox so there's no spacing between them.
  GtkWidget* back_forward_hbox_ = gtk_hbox_new(FALSE, 0);

  back_.reset(new BackForwardButtonGtk(browser_, false));
  gtk_box_pack_start(GTK_BOX(back_forward_hbox_), back_->widget(), FALSE,
                     FALSE, 0);
  g_signal_connect(back_->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);

  forward_.reset(new BackForwardButtonGtk(browser_, true));
  gtk_box_pack_start(GTK_BOX(back_forward_hbox_), forward_->widget(), FALSE,
                     FALSE, 0);
  g_signal_connect(forward_->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  gtk_box_pack_start(GTK_BOX(toolbar_), back_forward_hbox_, FALSE, FALSE, 0);

  reload_.reset(BuildToolbarButton(IDR_RELOAD, IDR_RELOAD_P, IDR_RELOAD_H, 0,
      l10n_util::GetStringUTF8(IDS_TOOLTIP_RELOAD),
      GTK_STOCK_REFRESH));

  home_.reset(BuildToolbarButton(IDR_HOME, IDR_HOME_P, IDR_HOME_H, 0,
                                 l10n_util::GetStringUTF8(IDS_TOOLTIP_HOME),
                                 GTK_STOCK_HOME));
  gtk_util::SetButtonTriggersNavigation(home_->widget());
  SetUpDragForHomeButton();

  // Group the start, omnibox, and go button into an hbox.
  GtkWidget* location_hbox = gtk_hbox_new(FALSE, 0);
  star_.reset(BuildStarButton(l10n_util::GetStringUTF8(IDS_TOOLTIP_STAR)));
  gtk_box_pack_start(GTK_BOX(location_hbox), star_->widget(), FALSE, FALSE, 0);

  location_bar_->Init(ShouldOnlyShowLocation());
  gtk_box_pack_start(GTK_BOX(location_hbox), location_bar_->widget(), TRUE,
                     TRUE, 0);

  go_.reset(new GoButtonGtk(location_bar_.get(), browser_));
  gtk_box_pack_start(GTK_BOX(location_hbox), go_->widget(), FALSE, FALSE, 0);

  g_signal_connect(location_hbox, "expose-event",
                   G_CALLBACK(OnLocationHboxExpose), this);
  gtk_box_pack_start(GTK_BOX(toolbar_), location_hbox, TRUE, TRUE,
                     ShouldOnlyShowLocation() ? 1 : 0);

  // Group the menu buttons together in an hbox.
  GtkWidget* menus_hbox_ = gtk_hbox_new(FALSE, 0);
  GtkWidget* page_menu = BuildToolbarMenuButton(
      l10n_util::GetStringUTF8(IDS_PAGEMENU_TOOLTIP),
      &page_menu_button_);
  page_menu_image_ = gtk_image_new_from_pixbuf(
      theme_provider_->GetRTLEnabledPixbufNamed(IDR_MENU_PAGE));
  gtk_container_add(GTK_CONTAINER(page_menu), page_menu_image_);

  encodings_menu_.reset(BuildEncodingsMenu(profile, this));
  page_menu_.reset(new MenuGtk(this, GetStandardPageMenu(encodings_menu_.get()),
                               accel_group_));
  g_signal_connect(page_menu_->widget(), "motion-notify-event",
                   G_CALLBACK(OnPageAppMenuMouseMotion), this);
  g_signal_connect(page_menu_->widget(), "move-current",
                   G_CALLBACK(OnPageAppMenuMoveCurrent), this);
  gtk_box_pack_start(GTK_BOX(menus_hbox_), page_menu, FALSE, FALSE, 0);

  GtkWidget* chrome_menu = BuildToolbarMenuButton(
      l10n_util::GetStringFUTF8(IDS_APPMENU_TOOLTIP,
          WideToUTF16(l10n_util::GetString(IDS_PRODUCT_NAME))),
      &app_menu_button_);
  app_menu_image_ = gtk_image_new_from_pixbuf(
      theme_provider_->GetRTLEnabledPixbufNamed(IDR_MENU_CHROME));
  gtk_container_add(GTK_CONTAINER(chrome_menu), app_menu_image_);
  app_menu_.reset(new MenuGtk(this, GetStandardAppMenu(), accel_group_));
  g_signal_connect(app_menu_->widget(), "motion-notify-event",
                   G_CALLBACK(OnPageAppMenuMouseMotion), this);
  g_signal_connect(app_menu_->widget(), "move-current",
                   G_CALLBACK(OnPageAppMenuMoveCurrent), this);
  gtk_box_pack_start(GTK_BOX(menus_hbox_), chrome_menu, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(toolbar_), menus_hbox_, FALSE, FALSE, 0);

  // Page and app menu accelerators.
  GtkAccelGroup* accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(top_level_window, accel_group);
  // Drop the initial ref on |accel_group| so |window_| will own it.
  g_object_unref(accel_group);
  // I would use "popup-menu" here, but GTK complains. I would use "activate",
  // but the docs say never to connect to that signal.
  gtk_widget_add_accelerator(page_menu, "clicked", accel_group,
                             GDK_e, GDK_MOD1_MASK,
                             static_cast<GtkAccelFlags>(0));
  gtk_widget_add_accelerator(chrome_menu, "clicked", accel_group,
                             GDK_f, GDK_MOD1_MASK,
                             static_cast<GtkAccelFlags>(0));

  if (ShouldOnlyShowLocation()) {
    gtk_widget_show(event_box_);
    gtk_widget_show(toolbar_);
    gtk_widget_show_all(location_hbox);
    gtk_widget_hide(star_->widget());
    gtk_widget_hide(go_->widget());
  } else {
    gtk_widget_show_all(event_box_);

    if (show_home_button_.GetValue()) {
      gtk_widget_show(home_->widget());
    } else {
      gtk_widget_hide(home_->widget());
    }
  }
}

void BrowserToolbarGtk::AddToolbarToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), event_box_, FALSE, FALSE, 0);
}

void BrowserToolbarGtk::Show() {
  gtk_widget_show(toolbar_);
}

void BrowserToolbarGtk::Hide() {
  gtk_widget_hide(toolbar_);
}

LocationBar* BrowserToolbarGtk::GetLocationBar() const {
  return location_bar_.get();
}

// CommandUpdater::CommandObserver ---------------------------------------------

void BrowserToolbarGtk::EnabledStateChangedForCommand(int id, bool enabled) {
  GtkWidget* widget = NULL;
  switch (id) {
    case IDC_BACK:
      widget = back_->widget();
      break;
    case IDC_FORWARD:
      widget = forward_->widget();
      break;
    case IDC_RELOAD:
      widget = reload_->widget();
      break;
    case IDC_GO:
      widget = go_->widget();
      break;
    case IDC_HOME:
      if (home_.get())
        widget = home_->widget();
      break;
    case IDC_STAR:
      widget = star_->widget();
      break;
  }
  if (widget)
    gtk_widget_set_sensitive(widget, enabled);
}

// MenuGtk::Delegate -----------------------------------------------------------

bool BrowserToolbarGtk::IsCommandEnabled(int command_id) const {
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool BrowserToolbarGtk::IsItemChecked(int id) const {
  if (!profile_)
    return false;

  EncodingMenuController controller;
  if (id == IDC_SHOW_BOOKMARK_BAR) {
    return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
  } else if (controller.DoesCommandBelongToEncodingMenu(id)) {
    TabContents* tab_contents = browser_->GetSelectedTabContents();
    if (tab_contents) {
      return controller.IsItemChecked(profile_, tab_contents->encoding(),
                                      id);
    }
  }

  return false;
}

void BrowserToolbarGtk::ExecuteCommand(int id) {
  browser_->ExecuteCommand(id);
}

void BrowserToolbarGtk::StoppedShowing() {
  gtk_chrome_button_unset_paint_state(
      GTK_CHROME_BUTTON(page_menu_button_.get()));
  gtk_chrome_button_unset_paint_state(
      GTK_CHROME_BUTTON(app_menu_button_.get()));
}

// NotificationObserver --------------------------------------------------------

void BrowserToolbarGtk::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (*pref_name == prefs::kShowHomeButton) {
      if (show_home_button_.GetValue() && !ShouldOnlyShowLocation()) {
        gtk_widget_show(home_->widget());
      } else {
        gtk_widget_hide(home_->widget());
      }
    }
  } else if (type == NotificationType::BROWSER_THEME_CHANGED) {
    // Update the spacing around the menu buttons
    int border = theme_provider_->UseGtkTheme() ? 0 : 2;
    gtk_container_set_border_width(
        GTK_CONTAINER(page_menu_button_.get()), border);
    gtk_container_set_border_width(
        GTK_CONTAINER(app_menu_button_.get()), border);

    // Update the menu button images.
    gtk_image_set_from_pixbuf(GTK_IMAGE(page_menu_image_),
        theme_provider_->GetRTLEnabledPixbufNamed(IDR_MENU_PAGE));
    gtk_image_set_from_pixbuf(GTK_IMAGE(app_menu_image_),
        theme_provider_->GetRTLEnabledPixbufNamed(IDR_MENU_CHROME));

    // When using the GTK+ theme, we need to have the event box be visible so
    // buttons don't get a halo color from the background.  When using Chromium
    // themes, we want to let the background show through the toolbar.
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_),
                                     theme_provider_->UseGtkTheme());
  } else {
    NOTREACHED();
  }
}

// BrowserToolbarGtk, public ---------------------------------------------------

void BrowserToolbarGtk::SetProfile(Profile* profile) {
  if (profile == profile_)
    return;

  profile_ = profile;
  location_bar_->SetProfile(profile);
}

void BrowserToolbarGtk::UpdateTabContents(TabContents* contents,
                                          bool should_restore_state) {
  location_bar_->Update(should_restore_state ? contents : NULL);
}

gfx::Rect BrowserToolbarGtk::GetPopupBounds() const {
  GtkWidget* left;
  GtkWidget* right;
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    left = go_->widget();
    right = star_->widget();
  } else {
    left = star_->widget();
    right = go_->widget();
  }

  // TODO(deanm): The go and star buttons probably share the same window,
  // so this could be optimized to only one origin request.
  gint right_x;
  gdk_window_get_origin(right->window, &right_x, NULL);
  right_x += right->allocation.x + right->allocation.width;

  gint left_x, left_y;
  gdk_window_get_origin(left->window, &left_x, &left_y);
  left_x += left->allocation.x;
  left_y += left->allocation.y + left->allocation.height;  // Bottom edge.

  return gfx::Rect(left_x + kPopupLeftRightMargin, left_y + kPopupTopMargin,
                   right_x - left_x - (2 * kPopupLeftRightMargin), 0);
}

// BrowserToolbarGtk, private --------------------------------------------------

CustomDrawButton* BrowserToolbarGtk::BuildToolbarButton(
    int normal_id, int active_id, int highlight_id, int depressed_id,
    const std::string& localized_tooltip, const char* stock_id) {
  CustomDrawButton* button = new CustomDrawButton(
      GtkThemeProvider::GetFrom(profile_),
      normal_id, active_id, highlight_id, depressed_id, stock_id,
      GTK_ICON_SIZE_SMALL_TOOLBAR);

  gtk_widget_set_tooltip_text(button->widget(),
                              localized_tooltip.c_str());
  g_signal_connect(button->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);

  gtk_box_pack_start(GTK_BOX(toolbar_), button->widget(), FALSE, FALSE, 0);
  return button;
}

ToolbarStarToggleGtk* BrowserToolbarGtk::BuildStarButton(
    const std::string& localized_tooltip) {
  ToolbarStarToggleGtk* button = new ToolbarStarToggleGtk(this);

  gtk_widget_set_tooltip_text(button->widget(),
                              localized_tooltip.c_str());
  g_signal_connect(button->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);

  return button;
}

GtkWidget* BrowserToolbarGtk::BuildToolbarMenuButton(
    const std::string& localized_tooltip,
    OwnedWidgetGtk* owner) {
  GtkWidget* button = theme_provider_->BuildChromeButton();
  owner->Own(button);

  gtk_widget_set_tooltip_text(button, localized_tooltip.c_str());
  g_signal_connect(button, "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEvent), this);
  g_signal_connect(button, "clicked",
                   G_CALLBACK(OnMenuClicked), this);
  GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);

  return button;
}

void BrowserToolbarGtk::SetUpDragForHomeButton() {
  // TODO(estade): we should use a custom drag-drop handler so that we can
  // prefer URIs over plain text when both are available.
  gtk_drag_dest_set(home_->widget(), GTK_DEST_DEFAULT_ALL,
                    NULL, 0, GDK_ACTION_COPY);
  static const int targets[] = { GtkDndUtil::TEXT_PLAIN,
                                 GtkDndUtil::TEXT_URI_LIST, -1 };
  GtkDndUtil::SetDestTargetList(home_->widget(), targets);

  g_signal_connect(home_->widget(), "drag-data-received",
                   G_CALLBACK(OnDragDataReceived), this);
}

void BrowserToolbarGtk::ChangeActiveMenu(GtkWidget* active_menu,
    guint timestamp) {
  MenuGtk* old_menu;
  MenuGtk* new_menu;
  GtkWidget* relevant_button;
  if (active_menu == app_menu_->widget()) {
    old_menu = app_menu_.get();
    new_menu = page_menu_.get();
    relevant_button = page_menu_button_.get();
  } else {
    old_menu = page_menu_.get();
    new_menu = app_menu_.get();
    relevant_button = app_menu_button_.get();
  }

  old_menu->Cancel();
  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(relevant_button),
                                    GTK_STATE_ACTIVE);
  new_menu->Popup(relevant_button, 0, timestamp);
}

// static
gboolean BrowserToolbarGtk::OnToolbarExpose(GtkWidget* widget,
                                            GdkEventExpose* e,
                                            BrowserToolbarGtk* toolbar) {
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, e->area.x, e->area.y, e->area.width, e->area.height);
  cairo_clip(cr);
  // The toolbar is supposed to blend in with the active tab, so we have to pass
  // coordinates for the IDR_THEME_TOOLBAR bitmap relative to the top of the
  // tab strip.
  gfx::Point tabstrip_origin =
      toolbar->window_->tabstrip()->GetTabStripOriginForWidget(widget);
  GtkThemeProvider* theme_provider = toolbar->theme_provider_;
  GdkPixbuf* toolbar_background = theme_provider->GetPixbufNamed(
      IDR_THEME_TOOLBAR);
  gdk_cairo_set_source_pixbuf(cr, toolbar_background, tabstrip_origin.x(),
                              tabstrip_origin.y());
  // We tile the toolbar background in both directions.
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr,
      tabstrip_origin.x(),
      tabstrip_origin.y(),
      e->area.x + e->area.width - tabstrip_origin.x(),
      e->area.y + e->area.height - tabstrip_origin.y());
  cairo_fill(cr);
  cairo_destroy(cr);

  return FALSE;  // Allow subwidgets to paint.
}

// static
gboolean BrowserToolbarGtk::OnLocationHboxExpose(GtkWidget* location_hbox,
                                                 GdkEventExpose* e,
                                                 BrowserToolbarGtk* toolbar) {
  if (toolbar->theme_provider_->UseGtkTheme()) {
    // To get the proper look surrounding the location bar, we issue raw gtk
    // painting commands to the theme engine. We figure out the region from the
    // leftmost widget to the rightmost and then tell GTK to perform the same
    // drawing commands that draw a GtkEntry on that region.
    GtkWidget* star = toolbar->star_->widget();
    GtkWidget* left = NULL;
    GtkWidget* right = NULL;
    if (toolbar->ShouldOnlyShowLocation()) {
      left = location_hbox;
      right = location_hbox;
    } else if (gtk_widget_get_direction(star) == GTK_TEXT_DIR_LTR) {
      left = toolbar->star_->widget();
      right = toolbar->go_->widget();
    } else {
      left = toolbar->go_->widget();
      right = toolbar->star_->widget();
    }

    GdkRectangle rec = {
      left->allocation.x,
      left->allocation.y,
      (right->allocation.x - left->allocation.x) + right->allocation.width,
      (right->allocation.y - left->allocation.y) + right->allocation.height
    };

    // Make sure our off screen entry has the correct base color if we're in
    // secure mode.
    gtk_widget_modify_base(
        toolbar->offscreen_entry_.get(), GTK_STATE_NORMAL,
        (toolbar->browser_->toolbar_model()->GetSchemeSecurityLevel() ==
         ToolbarModel::SECURE) ?
        &kSecureColor : NULL);

    GtkStyle* gtk_owned_style =
        gtk_rc_get_style(toolbar->offscreen_entry_.get());
    // GTK owns the above and we're going to have to make our own copy of it
    // that we can edit.
    GtkStyle* our_style = gtk_style_copy(gtk_owned_style);
    our_style = gtk_style_attach(our_style, location_hbox->window);

    // TODO(erg): Draw the focus ring if appropriate...

    // We're using GTK rendering; draw a GTK entry widget onto the background.
    gtk_paint_shadow(our_style, location_hbox->window,
                     GTK_STATE_NORMAL, GTK_SHADOW_IN, &rec,
                     location_hbox, "entry",
                     rec.x, rec.y, rec.width, rec.height);

    // Draw the interior background (not all themes draw the entry background
    // above; this is a noop on themes that do).
    gint xborder = our_style->xthickness;
    gint yborder = our_style->ythickness;
    gtk_paint_flat_box(our_style, location_hbox->window,
                       GTK_STATE_NORMAL, GTK_SHADOW_NONE, &rec,
                       location_hbox, "entry_bg",
                       rec.x + xborder, rec.y + yborder,
                       rec.width - 2 * xborder,
                       rec.height - 2 * yborder);

    g_object_unref(our_style);
  }

  return FALSE;
}

// static
void BrowserToolbarGtk::OnButtonClick(GtkWidget* button,
                                      BrowserToolbarGtk* toolbar) {
  if ((button == toolbar->back_->widget()) ||
      (button == toolbar->forward_->widget())) {
    toolbar->location_bar_->Revert();
    return;
  }

  int tag = -1;
  if (button == toolbar->reload_->widget()) {
    tag = IDC_RELOAD;
    toolbar->location_bar_->Revert();
  } else if (toolbar->home_.get() && button == toolbar->home_->widget()) {
    tag = IDC_HOME;
  } else if (button == toolbar->star_->widget()) {
    tag = IDC_STAR;
  }

  DCHECK_NE(tag, -1) << "Unexpected button click callback";
  toolbar->browser_->ExecuteCommandWithDisposition(tag,
      event_utils::DispositionFromEventFlags(
      reinterpret_cast<GdkEventButton*>(gtk_get_current_event())->state));
}

// static
gboolean BrowserToolbarGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                   GdkEventButton* event,
                                                   BrowserToolbarGtk* toolbar) {
  if (event->button != 1)
    return FALSE;

  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(button),
                                    GTK_STATE_ACTIVE);
  MenuGtk* menu = button == toolbar->page_menu_button_.get() ?
                  toolbar->page_menu_.get() : toolbar->app_menu_.get();
  menu->Popup(button, reinterpret_cast<GdkEvent*>(event));

  return TRUE;
}

// static
gboolean BrowserToolbarGtk::OnMenuClicked(GtkWidget* button,
                                          BrowserToolbarGtk* toolbar) {
  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(button),
                                    GTK_STATE_ACTIVE);
  MenuGtk* menu = button == toolbar->page_menu_button_.get() ?
                  toolbar->page_menu_.get() : toolbar->app_menu_.get();
  menu->PopupAsFromKeyEvent(button);

  return TRUE;
}

// static
void BrowserToolbarGtk::OnDragDataReceived(GtkWidget* widget,
    GdkDragContext* drag_context, gint x, gint y,
    GtkSelectionData* data, guint info, guint time,
    BrowserToolbarGtk* toolbar) {
  if (info != GtkDndUtil::TEXT_PLAIN) {
    NOTIMPLEMENTED() << "Only support plain text drops for now, sorry!";
    return;
  }

  GURL url(reinterpret_cast<char*>(data->data));
  if (!url.is_valid())
    return;

  bool url_is_newtab = url.spec() == chrome::kChromeUINewTabURL;
  toolbar->profile_->GetPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage,
                                            url_is_newtab);
  if (!url_is_newtab) {
    toolbar->profile_->GetPrefs()->SetString(prefs::kHomePage,
                                             UTF8ToWide(url.spec()));
  }
}

// static
gboolean BrowserToolbarGtk::OnPageAppMenuMouseMotion(GtkWidget* menu,
    GdkEventMotion* event, BrowserToolbarGtk* toolbar) {
  if (gtk_util::WidgetContainsCursor(menu == toolbar->app_menu_->widget() ?
                                     toolbar->page_menu_button_.get() :
                                     toolbar->app_menu_button_.get())) {
    toolbar->ChangeActiveMenu(menu, event->time);
    return TRUE;
  }

  return FALSE;
}

// static
void BrowserToolbarGtk::OnPageAppMenuMoveCurrent(GtkWidget* menu,
    GtkMenuDirectionType dir, BrowserToolbarGtk* toolbar) {
  GtkWidget* active_item = GTK_MENU_SHELL(menu)->active_menu_item;

  switch (dir) {
    case GTK_MENU_DIR_CHILD:
      // The move is going to open a submenu; don't override default behavior.
      if (active_item && gtk_menu_item_get_submenu(GTK_MENU_ITEM(active_item)))
        break;
      // Fall through.
    case GTK_MENU_DIR_PARENT:
      toolbar->ChangeActiveMenu(menu, gtk_get_current_event_time());
      // This signal doesn't have a return value; we have to manually stop its
      // propagation.
      g_signal_stop_emission_by_name(menu, "move-current");
    default:
      break;
  }
}

bool BrowserToolbarGtk::ShouldOnlyShowLocation() const {
  // If we're a popup window, only show the location bar (omnibox).
  return browser_->type() != Browser::TYPE_NORMAL;
}
