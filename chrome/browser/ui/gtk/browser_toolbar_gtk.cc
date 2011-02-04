// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"

#include <X11/XF86keysym.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background_page_tracker.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/accelerators_gtk.h"
#include "chrome/browser/ui/gtk/back_forward_button_gtk.h"
#include "chrome/browser/ui/gtk/browser_actions_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/cairo_cached_surface.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/gtk/reload_button_gtk.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "chrome/browser/ui/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "gfx/canvas_skia_paint.h"
#include "gfx/gtk_util.h"
#include "gfx/skbitmap_operations.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/accelerator_gtk.h"

namespace {

// Padding on left and right of the left toolbar buttons (back, forward, reload,
// etc.).
const int kToolbarLeftAreaPadding = 4;

// Height of the toolbar in pixels (not counting padding).
const int kToolbarHeight = 29;

// Padding within the toolbar above the buttons and location bar.
const int kTopBottomPadding = 3;

// Height of the toolbar in pixels when we only show the location bar.
const int kToolbarHeightLocationBarOnly = kToolbarHeight - 2;

// Interior spacing between toolbar widgets.
const int kToolbarWidgetSpacing = 1;

// Amount of rounding on top corners of toolbar. Only used in Gtk theme mode.
const int kToolbarCornerSize = 3;

void SetWidgetHeightRequest(GtkWidget* widget, gpointer user_data) {
  gtk_widget_set_size_request(widget, -1, GPOINTER_TO_INT(user_data));
}

}  // namespace

// BrowserToolbarGtk, public ---------------------------------------------------

BrowserToolbarGtk::BrowserToolbarGtk(Browser* browser, BrowserWindowGtk* window)
    : toolbar_(NULL),
      location_bar_(new LocationBarViewGtk(browser)),
      model_(browser->toolbar_model()),
      wrench_menu_model_(this, browser),
      browser_(browser),
      window_(window),
      profile_(NULL) {
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_BOOKMARK_PAGE, this);

  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::UPGRADE_RECOMMENDED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::BACKGROUND_PAGE_TRACKER_CHANGED,
                 NotificationService::AllSources());
}

BrowserToolbarGtk::~BrowserToolbarGtk() {
  browser_->command_updater()->RemoveCommandObserver(IDC_BACK, this);
  browser_->command_updater()->RemoveCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->RemoveCommandObserver(IDC_HOME, this);
  browser_->command_updater()->RemoveCommandObserver(IDC_BOOKMARK_PAGE, this);

  offscreen_entry_.Destroy();

  wrench_menu_.reset();
}

void BrowserToolbarGtk::Init(Profile* profile,
                             GtkWindow* top_level_window) {
  // Make sure to tell the location bar the profile before calling its Init.
  SetProfile(profile);

  theme_provider_ = GtkThemeProvider::GetFrom(profile);
  offscreen_entry_.Own(gtk_entry_new());

  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);
  home_page_.Init(prefs::kHomePage, profile->GetPrefs(), this);
  home_page_is_new_tab_page_.Init(prefs::kHomePageIsNewTabPage,
                                  profile->GetPrefs(), this);

  event_box_ = gtk_event_box_new();
  // Make the event box transparent so themes can use transparent toolbar
  // backgrounds.
  if (!theme_provider_->UseGtkTheme())
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_), FALSE);

  toolbar_ = gtk_hbox_new(FALSE, 0);
  alignment_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  UpdateForBookmarkBarVisibility(false);
  g_signal_connect(alignment_, "expose-event",
                   G_CALLBACK(&OnAlignmentExposeThunk), this);
  gtk_container_add(GTK_CONTAINER(event_box_), alignment_);
  gtk_container_add(GTK_CONTAINER(alignment_), toolbar_);

  toolbar_left_ = gtk_hbox_new(FALSE, kToolbarWidgetSpacing);

  back_.reset(new BackForwardButtonGtk(browser_, false));
  g_signal_connect(back_->widget(), "clicked",
                   G_CALLBACK(OnButtonClickThunk), this);
  gtk_box_pack_start(GTK_BOX(toolbar_left_), back_->widget(), FALSE,
                     FALSE, 0);

  forward_.reset(new BackForwardButtonGtk(browser_, true));
  g_signal_connect(forward_->widget(), "clicked",
                   G_CALLBACK(OnButtonClickThunk), this);
  gtk_box_pack_start(GTK_BOX(toolbar_left_), forward_->widget(), FALSE,
                     FALSE, 0);

  reload_.reset(new ReloadButtonGtk(location_bar_.get(), browser_));
  gtk_box_pack_start(GTK_BOX(toolbar_left_), reload_->widget(), FALSE, FALSE,
                     0);

  home_.reset(new CustomDrawButton(GtkThemeProvider::GetFrom(profile_),
      IDR_HOME, IDR_HOME_P, IDR_HOME_H, 0, GTK_STOCK_HOME,
      GTK_ICON_SIZE_SMALL_TOOLBAR));
  gtk_widget_set_tooltip_text(home_->widget(),
      l10n_util::GetStringUTF8(IDS_TOOLTIP_HOME).c_str());
  g_signal_connect(home_->widget(), "clicked",
                   G_CALLBACK(OnButtonClickThunk), this);
  gtk_box_pack_start(GTK_BOX(toolbar_left_), home_->widget(), FALSE, FALSE,
                     kToolbarWidgetSpacing);
  gtk_util::SetButtonTriggersNavigation(home_->widget());

  gtk_box_pack_start(GTK_BOX(toolbar_), toolbar_left_, FALSE, FALSE,
                     kToolbarLeftAreaPadding);

  location_hbox_ = gtk_hbox_new(FALSE, 0);
  location_bar_->Init(ShouldOnlyShowLocation());
  gtk_box_pack_start(GTK_BOX(location_hbox_), location_bar_->widget(), TRUE,
                     TRUE, 0);

  g_signal_connect(location_hbox_, "expose-event",
                   G_CALLBACK(OnLocationHboxExposeThunk), this);
  gtk_box_pack_start(GTK_BOX(toolbar_), location_hbox_, TRUE, TRUE,
      ShouldOnlyShowLocation() ? 1 : 0);

  if (!ShouldOnlyShowLocation()) {
    actions_toolbar_.reset(new BrowserActionsToolbarGtk(browser_));
    gtk_box_pack_start(GTK_BOX(toolbar_), actions_toolbar_->widget(),
                       FALSE, FALSE, 0);
  }

  wrench_menu_image_ = gtk_image_new_from_pixbuf(
      theme_provider_->GetRTLEnabledPixbufNamed(IDR_TOOLS));
  wrench_menu_button_.reset(new CustomDrawButton(
      GtkThemeProvider::GetFrom(profile_),
      IDR_TOOLS, IDR_TOOLS_P, IDR_TOOLS_H, 0,
      wrench_menu_image_));
  GtkWidget* wrench_button = wrench_menu_button_->widget();

  gtk_widget_set_tooltip_text(
      wrench_button,
      l10n_util::GetStringFUTF8(IDS_APPMENU_TOOLTIP,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)).c_str());
  g_signal_connect(wrench_button, "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEventThunk), this);
  GTK_WIDGET_UNSET_FLAGS(wrench_button, GTK_CAN_FOCUS);

  // Put the wrench button in a box so that we can paint the update notification
  // over it.
  GtkWidget* wrench_box = gtk_alignment_new(0, 0, 1, 1);
  g_signal_connect_after(wrench_box, "expose-event",
                         G_CALLBACK(OnWrenchMenuButtonExposeThunk), this);
  gtk_container_add(GTK_CONTAINER(wrench_box), wrench_button);
  gtk_box_pack_start(GTK_BOX(toolbar_), wrench_box, FALSE, FALSE, 4);

  wrench_menu_.reset(new MenuGtk(this, &wrench_menu_model_));
  registrar_.Add(this, NotificationType::ZOOM_LEVEL_CHANGED,
                 Source<Profile>(browser_->profile()));

  if (ShouldOnlyShowLocation()) {
    gtk_widget_show(event_box_);
    gtk_widget_show(alignment_);
    gtk_widget_show(toolbar_);
    gtk_widget_show_all(location_hbox_);
    gtk_widget_hide(reload_->widget());
  } else {
    gtk_widget_show_all(event_box_);
    if (actions_toolbar_->button_count() == 0)
      gtk_widget_hide(actions_toolbar_->widget());
  }
  // Initialize pref-dependent UI state.
  NotifyPrefChanged(NULL);

  // Because the above does a recursive show all on all widgets we need to
  // update the icon visibility to hide them.
  location_bar_->UpdateContentSettingsIcons();

  SetViewIDs();
  theme_provider_->InitThemesFor(this);
}

void BrowserToolbarGtk::SetViewIDs() {
  ViewIDUtil::SetID(widget(), VIEW_ID_TOOLBAR);
  ViewIDUtil::SetID(back_->widget(), VIEW_ID_BACK_BUTTON);
  ViewIDUtil::SetID(forward_->widget(), VIEW_ID_FORWARD_BUTTON);
  ViewIDUtil::SetID(reload_->widget(), VIEW_ID_RELOAD_BUTTON);
  ViewIDUtil::SetID(home_->widget(), VIEW_ID_HOME_BUTTON);
  ViewIDUtil::SetID(location_bar_->widget(), VIEW_ID_LOCATION_BAR);
  ViewIDUtil::SetID(wrench_menu_button_->widget(), VIEW_ID_APP_MENU);
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

void BrowserToolbarGtk::UpdateForBookmarkBarVisibility(
    bool show_bottom_padding) {
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment_),
      ShouldOnlyShowLocation() ? 0 : kTopBottomPadding,
      !show_bottom_padding || ShouldOnlyShowLocation() ? 0 : kTopBottomPadding,
      0, 0);
}

void BrowserToolbarGtk::ShowAppMenu() {
  wrench_menu_->Cancel();
  wrench_menu_button_->SetPaintOverride(GTK_STATE_ACTIVE);
  UserMetrics::RecordAction(UserMetricsAction("ShowAppMenu"));
  wrench_menu_->PopupAsFromKeyEvent(wrench_menu_button_->widget());
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
    case IDC_HOME:
      if (home_.get())
        widget = home_->widget();
      break;
  }
  if (widget) {
    if (!enabled && GTK_WIDGET_STATE(widget) == GTK_STATE_PRELIGHT) {
      // If we're disabling a widget, GTK will helpfully restore it to its
      // previous state when we re-enable it, even if that previous state
      // is the prelight.  This looks bad.  See the bug for a simple repro.
      // http://code.google.com/p/chromium/issues/detail?id=13729
      gtk_widget_set_state(widget, GTK_STATE_NORMAL);
    }
    gtk_widget_set_sensitive(widget, enabled);
  }
}

// MenuGtk::Delegate -----------------------------------------------------------

void BrowserToolbarGtk::StoppedShowing() {
  // Without these calls, the hover state can get stuck since the leave-notify
  // event is not sent when clicking a button brings up the menu.
  gtk_chrome_button_set_hover_state(
      GTK_CHROME_BUTTON(wrench_menu_button_->widget()), 0.0);
  wrench_menu_button_->UnsetPaintOverride();

  // Stop showing the BG page badge when we close the wrench menu.
  BackgroundPageTracker::GetInstance()->AcknowledgeBackgroundPages();
}

GtkIconSet* BrowserToolbarGtk::GetIconSetForId(int idr) {
  return theme_provider_->GetIconSetForId(idr);
}

// Always show images because we desire that some icons always show
// regardless of the system setting.
bool BrowserToolbarGtk::AlwaysShowIconForCmd(int command_id) const {
  return command_id == IDC_UPGRADE_DIALOG ||
         command_id == IDC_VIEW_BACKGROUND_PAGES;
}

// ui::AcceleratorProvider

bool BrowserToolbarGtk::GetAcceleratorForCommandId(
    int id,
    ui::Accelerator* accelerator) {
  const ui::AcceleratorGtk* accelerator_gtk =
      AcceleratorsGtk::GetInstance()->GetPrimaryAcceleratorForCommand(id);
  if (accelerator_gtk)
    *accelerator = *accelerator_gtk;
  return !!accelerator_gtk;
}

// NotificationObserver --------------------------------------------------------

void BrowserToolbarGtk::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    NotifyPrefChanged(Details<std::string>(details).ptr());
  } else if (type == NotificationType::BROWSER_THEME_CHANGED) {
    // Update the spacing around the menu buttons
    bool use_gtk = theme_provider_->UseGtkTheme();
    int border = use_gtk ? 0 : 2;
    gtk_container_set_border_width(
        GTK_CONTAINER(wrench_menu_button_->widget()), border);

    // Force the height of the toolbar so we get the right amount of padding
    // above and below the location bar. We always force the size of the widgets
    // to either side of the location box, but we only force the location box
    // size in chrome-theme mode because that's the only time we try to control
    // the font size.
    int toolbar_height = ShouldOnlyShowLocation() ?
                         kToolbarHeightLocationBarOnly : kToolbarHeight;
    gtk_container_foreach(GTK_CONTAINER(toolbar_), SetWidgetHeightRequest,
                          GINT_TO_POINTER(toolbar_height));
    gtk_widget_set_size_request(location_hbox_, -1,
                                use_gtk ? -1 : toolbar_height);

    // When using the GTK+ theme, we need to have the event box be visible so
    // buttons don't get a halo color from the background.  When using Chromium
    // themes, we want to let the background show through the toolbar.
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_), use_gtk);

    if (use_gtk) {
      // We need to manually update the icon if we are in GTK mode. (Note that
      // we set the initial value in Init()).
      gtk_image_set_from_pixbuf(
          GTK_IMAGE(wrench_menu_image_),
          theme_provider_->GetRTLEnabledPixbufNamed(IDR_TOOLS));
    }

    UpdateRoundedness();
  } else if (type == NotificationType::UPGRADE_RECOMMENDED ||
             type == NotificationType::BACKGROUND_PAGE_TRACKER_CHANGED) {
    // Redraw the wrench menu to update the badge.
    gtk_widget_queue_draw(wrench_menu_button_->widget());
  } else if (type == NotificationType::ZOOM_LEVEL_CHANGED) {
    // If our zoom level changed, we need to tell the menu to update its state,
    // since the menu could still be open.
    wrench_menu_->UpdateMenu();
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

  if (actions_toolbar_.get())
    actions_toolbar_->Update();
}

// BrowserToolbarGtk, private --------------------------------------------------

void BrowserToolbarGtk::SetUpDragForHomeButton(bool enable) {
  if (enable) {
    gtk_drag_dest_set(home_->widget(), GTK_DEST_DEFAULT_ALL,
                      NULL, 0, GDK_ACTION_COPY);
    static const int targets[] = { ui::TEXT_PLAIN, ui::TEXT_URI_LIST, -1 };
    ui::SetDestTargetList(home_->widget(), targets);

    drop_handler_.reset(new ui::GtkSignalRegistrar());
    drop_handler_->Connect(home_->widget(), "drag-data-received",
                           G_CALLBACK(OnDragDataReceivedThunk), this);
  } else {
    gtk_drag_dest_unset(home_->widget());
    drop_handler_.reset(NULL);
  }
}

bool BrowserToolbarGtk::UpdateRoundedness() {
  // We still round the corners if we are in chrome theme mode, but we do it by
  // drawing theme resources rather than changing the physical shape of the
  // widget.
  bool should_be_rounded = theme_provider_->UseGtkTheme() &&
      window_->ShouldDrawContentDropShadow();

  if (should_be_rounded == gtk_util::IsActingAsRoundedWindow(alignment_))
    return false;

  if (should_be_rounded) {
    gtk_util::ActAsRoundedWindow(alignment_, GdkColor(), kToolbarCornerSize,
                                 gtk_util::ROUNDED_TOP,
                                 gtk_util::BORDER_NONE);
  } else {
    gtk_util::StopActingAsRoundedWindow(alignment_);
  }

  return true;
}

gboolean BrowserToolbarGtk::OnAlignmentExpose(GtkWidget* widget,
                                              GdkEventExpose* e) {
  // We may need to update the roundedness of the toolbar's top corners. In
  // this case, don't draw; we'll be called again soon enough.
  if (UpdateRoundedness())
    return TRUE;

  // We don't need to render the toolbar image in GTK mode.
  if (theme_provider_->UseGtkTheme())
    return FALSE;

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  gdk_cairo_rectangle(cr, &e->area);
  cairo_clip(cr);

  gfx::Point tabstrip_origin =
      window_->tabstrip()->GetTabStripOriginForWidget(widget);
  // Fill the entire region with the toolbar color.
  GdkColor color = theme_provider_->GetGdkColor(
      BrowserThemeProvider::COLOR_TOOLBAR);
  gdk_cairo_set_source_color(cr, &color);
  cairo_fill(cr);

  // The horizontal size of the top left and right corner images.
  const int kCornerWidth = 4;
  // The thickness of the shadow outside the toolbar's bounds; the offset
  // between the edge of the toolbar and where we anchor the corner images.
  const int kShadowThickness = 2;

  gfx::Rect area(e->area);
  gfx::Rect right(widget->allocation.x + widget->allocation.width -
                      kCornerWidth,
                  widget->allocation.y - kShadowThickness,
                  kCornerWidth,
                  widget->allocation.height + kShadowThickness);
  gfx::Rect left(widget->allocation.x - kShadowThickness,
                 widget->allocation.y - kShadowThickness,
                 kCornerWidth,
                 widget->allocation.height + kShadowThickness);

  if (window_->ShouldDrawContentDropShadow()) {
    // Leave room to draw rounded corners.
    area = area.Subtract(right).Subtract(left);
  }

  CairoCachedSurface* background = theme_provider_->GetSurfaceNamed(
      IDR_THEME_TOOLBAR, widget);
  background->SetSource(cr, tabstrip_origin.x(), tabstrip_origin.y());
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr, area.x(), area.y(), area.width(), area.height());
  cairo_fill(cr);

  if (!window_->ShouldDrawContentDropShadow()) {
    // The rest of this function is for rounded corners. Our work is done here.
    cairo_destroy(cr);
    return FALSE;
  }

  bool draw_left_corner = left.Intersects(gfx::Rect(e->area));
  bool draw_right_corner = right.Intersects(gfx::Rect(e->area));

  if (draw_left_corner || draw_right_corner) {
    // Create a mask which is composed of the left and/or right corners.
    cairo_surface_t* target = cairo_surface_create_similar(
        cairo_get_target(cr),
        CAIRO_CONTENT_COLOR_ALPHA,
        widget->allocation.x + widget->allocation.width,
        widget->allocation.y + widget->allocation.height);
    cairo_t* copy_cr = cairo_create(target);

    cairo_set_operator(copy_cr, CAIRO_OPERATOR_SOURCE);
    if (draw_left_corner) {
      CairoCachedSurface* left_corner = theme_provider_->GetSurfaceNamed(
          IDR_CONTENT_TOP_LEFT_CORNER_MASK, widget);
      left_corner->SetSource(copy_cr, left.x(), left.y());
      cairo_paint(copy_cr);
    }
    if (draw_right_corner) {
      CairoCachedSurface* right_corner = theme_provider_->GetSurfaceNamed(
          IDR_CONTENT_TOP_RIGHT_CORNER_MASK, widget);
      right_corner->SetSource(copy_cr, right.x(), right.y());
      // We fill a path rather than just painting because we don't want to
      // overwrite the left corner.
      cairo_rectangle(copy_cr, right.x(), right.y(),
                      right.width(), right.height());
      cairo_fill(copy_cr);
    }

    // Draw the background. CAIRO_OPERATOR_IN uses the existing pixel data as
    // an alpha mask.
    background->SetSource(copy_cr, tabstrip_origin.x(), tabstrip_origin.y());
    cairo_set_operator(copy_cr, CAIRO_OPERATOR_IN);
    cairo_pattern_set_extend(cairo_get_source(copy_cr), CAIRO_EXTEND_REPEAT);
    cairo_paint(copy_cr);
    cairo_destroy(copy_cr);

    // Copy the temporary surface to the screen.
    cairo_set_source_surface(cr, target, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(target);
  }

  cairo_destroy(cr);

  return FALSE;  // Allow subwidgets to paint.
}

gboolean BrowserToolbarGtk::OnLocationHboxExpose(GtkWidget* location_hbox,
                                                 GdkEventExpose* e) {
  if (theme_provider_->UseGtkTheme()) {
    gtk_util::DrawTextEntryBackground(offscreen_entry_.get(),
                                      location_hbox, &e->area,
                                      &location_hbox->allocation);
  }

  return FALSE;
}

void BrowserToolbarGtk::OnButtonClick(GtkWidget* button) {
  if ((button == back_->widget()) || (button == forward_->widget())) {
    if (gtk_util::DispositionForCurrentButtonPressEvent() == CURRENT_TAB)
      location_bar_->Revert();
    return;
  }

  DCHECK(home_.get() && button == home_->widget()) <<
      "Unexpected button click callback";
  browser_->Home(gtk_util::DispositionForCurrentButtonPressEvent());
}

gboolean BrowserToolbarGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                   GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;

  wrench_menu_button_->SetPaintOverride(GTK_STATE_ACTIVE);
  wrench_menu_->PopupForWidget(button, event->button, event->time);

  return TRUE;
}

void BrowserToolbarGtk::OnDragDataReceived(GtkWidget* widget,
    GdkDragContext* drag_context, gint x, gint y,
    GtkSelectionData* data, guint info, guint time) {
  if (info != ui::TEXT_PLAIN) {
    NOTIMPLEMENTED() << "Only support plain text drops for now, sorry!";
    return;
  }

  GURL url(reinterpret_cast<char*>(data->data));
  if (!url.is_valid())
    return;

  bool url_is_newtab = url.spec() == chrome::kChromeUINewTabURL;
  home_page_is_new_tab_page_.SetValue(url_is_newtab);
  if (!url_is_newtab)
    home_page_.SetValue(url.spec());
}

void BrowserToolbarGtk::NotifyPrefChanged(const std::string* pref) {
  if (!pref || *pref == prefs::kShowHomeButton) {
    if (show_home_button_.GetValue() && !ShouldOnlyShowLocation()) {
      gtk_widget_show(home_->widget());
    } else {
      gtk_widget_hide(home_->widget());
    }
  }

  if (!pref ||
      *pref == prefs::kHomePage ||
      *pref == prefs::kHomePageIsNewTabPage)
    SetUpDragForHomeButton(!home_page_.IsManaged() &&
                           !home_page_is_new_tab_page_.IsManaged());
}

bool BrowserToolbarGtk::ShouldOnlyShowLocation() const {
  // If we're a popup window, only show the location bar (omnibox).
  return browser_->type() != Browser::TYPE_NORMAL;
}

gboolean BrowserToolbarGtk::OnWrenchMenuButtonExpose(GtkWidget* sender,
                                                     GdkEventExpose* expose) {
  const SkBitmap* badge = NULL;
  if (UpgradeDetector::GetInstance()->notify_upgrade()) {
    badge = theme_provider_->GetBitmapNamed(IDR_UPDATE_BADGE);
  } else if (BackgroundPageTracker::GetInstance()->
             GetUnacknowledgedBackgroundPageCount()) {
    badge = theme_provider_->GetBitmapNamed(IDR_BACKGROUND_BADGE);
  } else {
    return FALSE;
  }

  // Draw the chrome app menu icon onto the canvas.
  gfx::CanvasSkiaPaint canvas(expose, false);
  int x_offset = base::i18n::IsRTL() ? 0 :
      sender->allocation.width - badge->width();
  int y_offset = 0;
  canvas.DrawBitmapInt(
      *badge,
      sender->allocation.x + x_offset,
      sender->allocation.y + y_offset);

  return FALSE;
}
