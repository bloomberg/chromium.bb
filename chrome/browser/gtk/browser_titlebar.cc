// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_titlebar.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "app/gfx/gtk_util.h"
#include "app/gfx/skbitmap_operations.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/encoding_menu_controller.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The space above the titlebars.
const int kTitlebarHeight = 14;

// A linux specific menu item for toggling window decorations.
const int kShowWindowDecorationsCommand = 200;

// The following OTR constants copied from opaque_browser_frame_view.cc:
// In maximized mode, the OTR avatar starts 2 px below the top of the screen, so
// that it doesn't extend into the "3D edge" portion of the titlebar.
const int kOTRMaximizedTopSpacing = 2;
// The OTR avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kOTRBottomSpacing = 2;
// There are 2 px on each side of the OTR avatar (between the frame border and
// it on the left, and between it and the tabstrip on the right).
const int kOTRSideSpacing = 2;

// The thickness of the custom frame border; we need it here to enlarge the
// close button whent the custom frame border isn't showing but the custom
// titlebar is showing.
const int kFrameBorderThickness = 4;

// There is a 4px gap between the icon and the title text.
const int kIconTitleSpacing = 4;

// Padding around the icon when in app mode or popup mode.
const int kAppModePaddingTop = 5;
const int kAppModePaddingBottom = 4;
const int kAppModePaddingLeft = 2;

// The left padding of the tab strip.  In Views, the tab strip has a left
// margin of FrameBorderThickness + kClientEdgeThickness.  This offset is to
// account for kClientEdgeThickness.
const int kTabStripLeftPadding = 1;

// Spacing between buttons of the titlebar.
const int kButtonSpacing = 2;

// Spacing around outside of titlebar buttons.
const int kButtonOuterPadding = 2;

gboolean OnMouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                          BrowserWindowGtk* browser_window) {
  // Reset to the default mouse cursor.
  browser_window->ResetCustomFrameCursor();
  return TRUE;
}

GdkPixbuf* GetOTRAvatar() {
  static GdkPixbuf* otr_avatar = NULL;
  if (!otr_avatar) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    otr_avatar = rb.GetRTLEnabledPixbufNamed(IDR_OTR_ICON);
  }
  return otr_avatar;
}

// Converts a GdkColor to a color_utils::HSL.
color_utils::HSL GdkColorToHSL(const GdkColor* color) {
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(SkColorSetRGB(color->red >> 8,
                                          color->green >> 8,
                                          color->blue >> 8), &hsl);
  return hsl;
}

// Returns either |one| or |two| based on which has a greater difference in
// luminosity.
GdkColor PickLuminosityContrastingColor(const GdkColor* base,
                                        const GdkColor* one,
                                        const GdkColor* two) {
  // Convert all GdkColors to color_utils::HSLs.
  color_utils::HSL baseHSL = GdkColorToHSL(base);
  color_utils::HSL oneHSL = GdkColorToHSL(one);
  color_utils::HSL twoHSL = GdkColorToHSL(two);
  double one_difference = fabs(baseHSL.l - oneHSL.l);
  double two_difference = fabs(baseHSL.l - twoHSL.l);

  // Be biased towards the first color presented.
  if (two_difference > one_difference + 0.1)
    return *two;
  else
    return *one;
}

MenuCreateMaterial g_favicon_menu[] = {
  { MENU_NORMAL, IDC_BACK, IDS_CONTENT_CONTEXT_BACK, 0, NULL,
    GDK_Left, GDK_MOD1_MASK },
  { MENU_NORMAL, IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD, 0, NULL,
    GDK_Right, GDK_MOD1_MASK },
  { MENU_NORMAL, IDC_RELOAD, IDS_APP_MENU_RELOAD, 0, NULL,
    GDK_R, GDK_CONTROL_MASK },
  { MENU_SEPARATOR },
  { MENU_NORMAL, IDC_RESTORE_TAB, IDS_RESTORE_TAB, 0, NULL,
    GDK_T, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
  { MENU_NORMAL, IDC_DUPLICATE_TAB, IDS_APP_MENU_DUPLICATE_APP_WINDOW },
  { MENU_NORMAL, IDC_COPY_URL, IDS_APP_MENU_COPY_URL },
  { MENU_NORMAL, IDC_SHOW_AS_TAB, IDS_SHOW_AS_TAB },
  { MENU_NORMAL, IDC_NEW_TAB, IDS_APP_MENU_NEW_WEB_PAGE, 0, NULL,
    GDK_T, GDK_CONTROL_MASK },
};

const MenuCreateMaterial* GetFaviconMenu(Profile* profile,
                                         MenuGtk::Delegate* delegate) {
  static bool favicon_menu_built = false;
  static MenuCreateMaterial* favicon_menu;
  if (!favicon_menu_built) {
    const MenuCreateMaterial* standard_page =
        GetStandardPageMenu(profile, delegate);
    int standard_page_menu_length = 1;
    // Don't include the Create App Shortcut menu item.
    int start_offset = 0;
    for (int i = 0; standard_page[i].type != MENU_END; ++i) {
      if (standard_page[i].id == IDC_CREATE_SHORTCUTS) {
        // Pass the separator as well.
        start_offset = i + 2;
        ++i;
        continue;
      } else if (start_offset == 0) {
        // The Create App Shortcut menu item is the first menu item, and if that
        // ever changes we'll probably have to re-evaluate this code.
        NOTREACHED();
        continue;
      }

      standard_page_menu_length++;
    }
    favicon_menu = new MenuCreateMaterial[arraysize(g_favicon_menu) +
                                          standard_page_menu_length];
    memcpy(favicon_menu, g_favicon_menu,
           arraysize(g_favicon_menu) * sizeof(MenuCreateMaterial));
    memcpy(favicon_menu + arraysize(g_favicon_menu),
           standard_page + start_offset,
           (standard_page_menu_length) * sizeof(MenuCreateMaterial));
  }
  return favicon_menu;
}

}  // namespace

BrowserTitlebar::BrowserTitlebar(BrowserWindowGtk* browser_window,
                                 GtkWindow* window)
    : browser_window_(browser_window), window_(window),
      app_mode_favicon_(NULL),
      app_mode_title_(NULL),
      using_custom_frame_(false),
      window_has_focus_(false),
      theme_provider_(NULL) {
  Init();
}

void BrowserTitlebar::Init() {
  // The widget hierarchy is shown below.
  //
  // +- EventBox (container_) -------------------------------------------------+
  // +- HBox (container_hbox) -------------------------------------------------+
  // |+- Algn. -++- Alignment --------------++- VBox (titlebar_buttons_box_) -+|
  // ||+ Image +||   (titlebar_alignment_)  ||+ - Fixed (top_padding_) ------+||
  // |||       |||                          ||+- HBox -----------------------+||
  // |||spy_guy|||                          |||+- button -++- button -+      |||
  // |||       |||+- TabStripGtk  ---------+|||| minimize || restore  | ...  |||
  // |||  )8\  |||| tab   tab   tabclose   ||||+----------++----------+      |||
  // ||+-------+||+------------------------+||+------------------------------+||
  // |+---------++--------------------------++--------------------------------+|
  // +-------------------------------------------------------------------------+
  //
  // If we're a popup window or in app mode, we don't display the spy guy or
  // the tab strip.  Instead, put an hbox in titlebar_alignment_ in place of
  // the tab strip.
  // +- Alignment (titlebar_alignment_) -----------------------------------+
  // |+ HBox -------------------------------------------------------------+|
  // ||+- TabStripGtk -++- Image ----------------++- Label --------------+||
  // ||| hidden        ++    (app_mode_favicon_) ||    (app_mode_title_) |||
  // |||               ||  favicon               ||  page title          |||
  // ||+---------------++------------------------++----------------------+||
  // |+-------------------------------------------------------------------+|
  // +---------------------------------------------------------------------+
  GtkWidget* container_hbox = gtk_hbox_new(FALSE, 0);

  container_ = gtk_event_box_new();
  gtk_widget_set_name(container_, "chrome-browser-titlebar");
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(container_), FALSE);
  gtk_container_add(GTK_CONTAINER(container_), container_hbox);

  g_signal_connect(G_OBJECT(container_), "scroll-event",
                   G_CALLBACK(OnScroll), this);

  g_signal_connect(window_, "window-state-event",
                   G_CALLBACK(OnWindowStateChanged), this);

  if (browser_window_->browser()->profile()->IsOffTheRecord() &&
      browser_window_->browser()->type() == Browser::TYPE_NORMAL) {
    GtkWidget* spy_guy = gtk_image_new_from_pixbuf(GetOTRAvatar());
    gtk_misc_set_alignment(GTK_MISC(spy_guy), 0.0, 1.0);
    GtkWidget* spy_frame = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    // We use this alignment rather than setting padding on the GtkImage because
    // the image's intrinsic padding doesn't clip the pixbuf during painting.
    gtk_alignment_set_padding(GTK_ALIGNMENT(spy_frame), kOTRMaximizedTopSpacing,
        kOTRBottomSpacing, kOTRSideSpacing, kOTRSideSpacing);
    gtk_widget_set_size_request(spy_guy, -1, 0);
    gtk_container_add(GTK_CONTAINER(spy_frame), spy_guy);
    gtk_box_pack_start(GTK_BOX(container_hbox), spy_frame, FALSE, FALSE, 0);
  }

  // We use an alignment to control the titlebar height.
  titlebar_alignment_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  if (browser_window_->browser()->type() == Browser::TYPE_NORMAL) {
    gtk_box_pack_start(GTK_BOX(container_hbox), titlebar_alignment_, TRUE,
                       TRUE, 0);

    // Put the tab strip in the titlebar.
    gtk_container_add(GTK_CONTAINER(titlebar_alignment_),
                      browser_window_->tabstrip()->widget());
  } else {
    // App mode specific widgets.
    gtk_box_pack_start(GTK_BOX(container_hbox), titlebar_alignment_, TRUE,
                       TRUE, 0);
    GtkWidget* app_mode_hbox = gtk_hbox_new(FALSE, kIconTitleSpacing);
    gtk_container_add(GTK_CONTAINER(titlebar_alignment_), app_mode_hbox);

    // Put the tab strip in the hbox even though we don't show it.  Sometimes
    // we need the position of the tab strip so make sure it's in our widget
    // hierarchy.
    gtk_box_pack_start(GTK_BOX(app_mode_hbox),
        browser_window_->tabstrip()->widget(), FALSE, FALSE, 0);

    GtkWidget* favicon_event_box = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(favicon_event_box), FALSE);
    g_signal_connect(favicon_event_box, "button-press-event",
                     G_CALLBACK(OnButtonPressed), this);
    gtk_box_pack_start(GTK_BOX(app_mode_hbox), favicon_event_box, FALSE,
                       FALSE, 0);
    // We use the app logo as a placeholder image so the title doesn't jump
    // around.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    app_mode_favicon_ = gtk_image_new_from_pixbuf(
        rb.GetRTLEnabledPixbufNamed(IDR_PRODUCT_LOGO_16));
    g_object_set_data(G_OBJECT(app_mode_favicon_), "left-align-popup",
                      reinterpret_cast<void*>(true));
    gtk_container_add(GTK_CONTAINER(favicon_event_box), app_mode_favicon_);

    app_mode_title_ = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(app_mode_title_), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment(GTK_MISC(app_mode_title_), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(app_mode_hbox), app_mode_title_, TRUE, TRUE,
                       0);

    // Register with the theme provider to set the |app_mode_title_| label
    // color.
    theme_provider_ = GtkThemeProvider::GetFrom(
        browser_window_->browser()->profile());
    registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                   NotificationService::AllSources());
    theme_provider_->InitThemesFor(this);
    UpdateTitleAndIcon();
  }

  // We put the min/max/restore/close buttons in a vbox so they are top aligned
  // (up to padding) and don't vertically stretch.
  titlebar_buttons_box_ = gtk_vbox_new(FALSE, 0);
  GtkWidget* buttons_hbox = gtk_hbox_new(FALSE, kButtonSpacing);
  top_padding_ = gtk_fixed_new();
  gtk_widget_set_size_request(top_padding_, -1, kButtonOuterPadding);
  gtk_box_pack_start(GTK_BOX(titlebar_buttons_box_), top_padding_, FALSE, FALSE,
                     0);
  gtk_box_pack_start(GTK_BOX(titlebar_buttons_box_), buttons_hbox, FALSE,
                     FALSE, 0);

  if (CommandLine::ForCurrentProcess()->HasSwitch("glen")) {
    close_button_.reset(BuildTitlebarButton(IDR_GLEN, IDR_GLEN, IDR_GLEN,
                                            buttons_hbox, IDS_GLEN));
  } else {
    close_button_.reset(BuildTitlebarButton(IDR_CLOSE, IDR_CLOSE_P, IDR_CLOSE_H,
                                            buttons_hbox,
                                            IDS_XPFRAME_CLOSE_TOOLTIP));
  }

  restore_button_.reset(BuildTitlebarButton(IDR_RESTORE, IDR_RESTORE_P,
                        IDR_RESTORE_H, buttons_hbox,
                        IDS_XPFRAME_RESTORE_TOOLTIP));
  maximize_button_.reset(BuildTitlebarButton(IDR_MAXIMIZE, IDR_MAXIMIZE_P,
                         IDR_MAXIMIZE_H, buttons_hbox,
                         IDS_XPFRAME_MAXIMIZE_TOOLTIP));
  minimize_button_.reset(BuildTitlebarButton(IDR_MINIMIZE, IDR_MINIMIZE_P,
                         IDR_MINIMIZE_H, buttons_hbox,
                         IDS_XPFRAME_MINIMIZE_TOOLTIP));

  gtk_util::SetButtonClickableByMouseButtons(maximize_button_->widget(),
                                             true, true, true);

  gtk_widget_size_request(close_button_->widget(), &close_button_req_);
  gtk_widget_size_request(minimize_button_->widget(), &minimize_button_req_);
  gtk_widget_size_request(restore_button_->widget(), &restore_button_req_);

  gtk_box_pack_end(GTK_BOX(container_hbox), titlebar_buttons_box_, FALSE,
                   FALSE, 0);

  gtk_widget_show_all(container_);

  ActiveWindowWatcherX::AddObserver(this);
}

BrowserTitlebar::~BrowserTitlebar() {
  ActiveWindowWatcherX::RemoveObserver(this);
}

CustomDrawButton* BrowserTitlebar::BuildTitlebarButton(int image,
    int image_pressed, int image_hot, GtkWidget* box, int tooltip) {
  CustomDrawButton* button = new CustomDrawButton(image, image_pressed,
                                                  image_hot, 0);
  gtk_widget_add_events(GTK_WIDGET(button->widget()), GDK_POINTER_MOTION_MASK);
  g_signal_connect(button->widget(), "clicked", G_CALLBACK(OnButtonClicked),
                   this);
  g_signal_connect(button->widget(), "motion-notify-event",
                   G_CALLBACK(OnMouseMoveEvent), browser_window_);
  std::string localized_tooltip = l10n_util::GetStringUTF8(tooltip);
  gtk_widget_set_tooltip_text(button->widget(),
                              localized_tooltip.c_str());
  gtk_box_pack_end(GTK_BOX(box), button->widget(), FALSE, FALSE, 0);
  return button;
}

void BrowserTitlebar::UpdateCustomFrame(bool use_custom_frame) {
  using_custom_frame_ = use_custom_frame;
  if (use_custom_frame)
    gtk_widget_show(titlebar_buttons_box_);
  else
    gtk_widget_hide(titlebar_buttons_box_);
  UpdateTitlebarAlignment();
}

void BrowserTitlebar::UpdateTitleAndIcon() {
  if (!app_mode_title_)
    return;

  // Get the page title and elide it to the available space.
  string16 title = browser_window_->browser()->GetWindowTitleForCurrentTab();
  gtk_label_set_text(GTK_LABEL(app_mode_title_), UTF16ToUTF8(title).c_str());

  if (browser_window_->browser()->type() == Browser::TYPE_APP) {
    // Update the system app icon.  We don't need to update the icon in the top
    // left of the custom frame, that will get updated when the throbber is
    // updated.
    SkBitmap icon = browser_window_->browser()->GetCurrentPageIcon();
    if (icon.empty()) {
      gtk_util::SetWindowIcon(window_);
    } else {
      GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
      gtk_window_set_icon(window_, icon_pixbuf);
      g_object_unref(icon_pixbuf);
    }
  }
}

void BrowserTitlebar::UpdateThrobber(TabContents* tab_contents) {
  DCHECK(app_mode_favicon_);

  if (tab_contents && tab_contents->is_loading()) {
    GdkPixbuf* icon_pixbuf =
        throbber_.GetNextFrame(tab_contents->waiting_for_response());
    gtk_image_set_from_pixbuf(GTK_IMAGE(app_mode_favicon_), icon_pixbuf);
  } else {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    if (browser_window_->browser()->type() == Browser::TYPE_APP) {
      SkBitmap icon = browser_window_->browser()->GetCurrentPageIcon();
      if (icon.empty()) {
        // Fallback to the Chromium icon if the page has no icon.
        gtk_image_set_from_pixbuf(GTK_IMAGE(app_mode_favicon_),
            rb.GetPixbufNamed(IDR_PRODUCT_LOGO_16));
      } else {
        GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
        gtk_image_set_from_pixbuf(GTK_IMAGE(app_mode_favicon_), icon_pixbuf);
        g_object_unref(icon_pixbuf);
      }
    } else {
      gtk_image_set_from_pixbuf(GTK_IMAGE(app_mode_favicon_),
          rb.GetPixbufNamed(IDR_PRODUCT_LOGO_16));
    }
    throbber_.Reset();
  }
}

void BrowserTitlebar::UpdateTitlebarAlignment() {
  if (browser_window_->browser()->type() == Browser::TYPE_NORMAL) {
    if (using_custom_frame_ && !browser_window_->IsMaximized()) {
      gtk_alignment_set_padding(GTK_ALIGNMENT(titlebar_alignment_),
          kTitlebarHeight, 0, kTabStripLeftPadding, 0);
    } else {
      gtk_alignment_set_padding(GTK_ALIGNMENT(titlebar_alignment_), 0, 0,
                                kTabStripLeftPadding, 0);
    }
  } else {
    if (using_custom_frame_ && !browser_window_->IsFullscreen()) {
      gtk_alignment_set_padding(GTK_ALIGNMENT(titlebar_alignment_),
          kAppModePaddingTop, kAppModePaddingBottom, kAppModePaddingLeft, 0);
      gtk_widget_show(titlebar_alignment_);
    } else {
      gtk_widget_hide(titlebar_alignment_);
    }
  }

  // Resize the buttons so that the clickable area extends all the way to the
  // edge of the browser window.
  GtkRequisition close_button_req = close_button_req_;
  GtkRequisition minimize_button_req = minimize_button_req_;
  GtkRequisition restore_button_req = restore_button_req_;
  if (using_custom_frame_ && browser_window_->IsMaximized()) {
    close_button_req.width += kButtonOuterPadding;
    close_button_req.height += kButtonOuterPadding;
    minimize_button_req.height += kButtonOuterPadding;
    restore_button_req.height += kButtonOuterPadding;
    gtk_widget_hide(top_padding_);
  } else {
    gtk_widget_show(top_padding_);
  }
  gtk_widget_set_size_request(close_button_->widget(),
                              close_button_req.width, close_button_req.height);
  gtk_widget_set_size_request(minimize_button_->widget(),
                              minimize_button_req.width,
                              minimize_button_req.height);
  gtk_widget_set_size_request(restore_button_->widget(),
                              restore_button_req.width,
                              restore_button_req.height);
}

void BrowserTitlebar::UpdateTextColor() {
  if (app_mode_title_) {
    if (theme_provider_ && theme_provider_->UseGtkTheme()) {
      // We don't really have any good options here.
      //
      // Colors from window manager themes aren't exposed in GTK; the window
      // manager is a separate component and when there is information sharing
      // (in the case of metacity), it's one way where the window manager reads
      // data from the GTK theme (which allows us to do a decent job with
      // picking the frame color).
      //
      // We probably won't match in the majority of cases, but we can at the
      // very least make things legible. The default metacity and xfwm themes
      // on ubuntu have white text hardcoded. Determine whether black or white
      // has more luminosity contrast and then set that color as the text
      // color.
      GdkColor frame_color;
      if (window_has_focus_) {
        frame_color = theme_provider_->GetGdkColor(
          BrowserThemeProvider::COLOR_FRAME);
      } else {
        frame_color = theme_provider_->GetGdkColor(
          BrowserThemeProvider::COLOR_FRAME_INACTIVE);
      }
      GdkColor text_color = PickLuminosityContrastingColor(
          &frame_color, &gfx::kGdkWhite, &gfx::kGdkBlack);
      gtk_util::SetLabelColor(app_mode_title_, &text_color);
    } else {
      // TODO(tc): Seems like this color should be themable, but it's hardcoded
      // to white on Windows.  http://crbug.com/18093
      gtk_util::SetLabelColor(app_mode_title_, &gfx::kGdkWhite);
    }
  }
}

void BrowserTitlebar::ShowFaviconMenu(GdkEventButton* event) {
  if (!favicon_menu_.get()) {
    favicon_menu_.reset(new MenuGtk(this,
        GetFaviconMenu(browser_window_->browser()->profile(), this)));
  }

  favicon_menu_->Popup(app_mode_favicon_, reinterpret_cast<GdkEvent*>(event));
}

void BrowserTitlebar::MaximizeButtonClicked() {
  GdkEventButton* event = &gtk_get_current_event()->button;
  if (event->button == 1) {
    gtk_window_maximize(window_);
  } else {
    GtkWidget* widget = GTK_WIDGET(window_);
    GdkScreen* screen = gtk_widget_get_screen(widget);
    gint monitor = gdk_screen_get_monitor_at_window(screen, widget->window);
    GdkRectangle screen_rect;
    gdk_screen_get_monitor_geometry(screen, monitor, &screen_rect);

    gint x, y;
    gtk_window_get_position(window_, &x, &y);
    gint width = widget->allocation.width;
    gint height = widget->allocation.height;

    if (event->button == 3) {
      x = 0;
      width = screen_rect.width;
    } else if (event->button == 2) {
      y = 0;
      height = screen_rect.height;
    }

    browser_window_->SetBounds(gfx::Rect(x, y, width, height));
  }
}

// static
gboolean BrowserTitlebar::OnWindowStateChanged(GtkWindow* window,
    GdkEventWindowState* event, BrowserTitlebar* titlebar) {
  // Update the maximize/restore button.
  if (titlebar->browser_window_->IsMaximized()) {
    gtk_widget_hide(titlebar->maximize_button_->widget());
    gtk_widget_show(titlebar->restore_button_->widget());
  } else {
    gtk_widget_hide(titlebar->restore_button_->widget());
    gtk_widget_show(titlebar->maximize_button_->widget());
  }
  titlebar->UpdateTitlebarAlignment();
  titlebar->UpdateTextColor();
  return FALSE;
}

// static
gboolean BrowserTitlebar::OnScroll(GtkWidget* widget, GdkEventScroll* event,
                                   BrowserTitlebar* titlebar) {
  TabStripModel* tabstrip_model =
      titlebar->browser_window_->browser()->tabstrip_model();
  int index = tabstrip_model->selected_index();
  if (event->direction == GDK_SCROLL_LEFT ||
      event->direction == GDK_SCROLL_UP) {
    if (index != 0)
      tabstrip_model->SelectPreviousTab();
  } else if (index + 1 < tabstrip_model->count()) {
    tabstrip_model->SelectNextTab();
  }
  return TRUE;
}

// static
void BrowserTitlebar::OnButtonClicked(GtkWidget* button,
                                      BrowserTitlebar* titlebar) {
  if (titlebar->close_button_->widget() == button) {
    titlebar->browser_window_->Close();
  } else if (titlebar->restore_button_->widget() == button) {
    titlebar->browser_window_->UnMaximize();
  } else if (titlebar->maximize_button_->widget() == button) {
    titlebar->MaximizeButtonClicked();
  } else if (titlebar->minimize_button_->widget() == button) {
    gtk_window_iconify(titlebar->window_);
  }
}

// static
gboolean BrowserTitlebar::OnButtonPressed(GtkWidget* widget,
                                          GdkEventButton* event,
                                          BrowserTitlebar* titlebar) {
  if (event->button != 1)
    return FALSE;

  titlebar->ShowFaviconMenu(event);
  return TRUE;
}

void BrowserTitlebar::ShowContextMenu() {
  if (!context_menu_.get()) {
    static const MenuCreateMaterial context_menu_blueprint[] = {
        { MENU_NORMAL, IDC_NEW_TAB, IDS_TAB_CXMENU_NEWTAB, 0, NULL,
            GDK_t, GDK_CONTROL_MASK },
        { MENU_NORMAL, IDC_RESTORE_TAB, IDS_RESTORE_TAB, 0, NULL,
            GDK_t, GDK_CONTROL_MASK | GDK_SHIFT_MASK },
        { MENU_SEPARATOR },
        { MENU_NORMAL, IDC_TASK_MANAGER, IDS_TASK_MANAGER, 0, NULL,
            GDK_Escape, GDK_SHIFT_MASK },
        { MENU_SEPARATOR },
        { MENU_CHECKBOX, kShowWindowDecorationsCommand,
            IDS_SHOW_WINDOW_DECORATIONS_MENU },
        { MENU_END },
    };

    context_menu_.reset(new MenuGtk(this, context_menu_blueprint));
  }

  context_menu_->PopupAsContext(gtk_get_current_event_time());
}

bool BrowserTitlebar::IsCommandEnabled(int command_id) const {
  if (command_id == kShowWindowDecorationsCommand)
    return true;

  return browser_window_->browser()->command_updater()->
      IsCommandEnabled(command_id);
}

bool BrowserTitlebar::IsItemChecked(int command_id) const {
  if (command_id == kShowWindowDecorationsCommand) {
    PrefService* prefs = browser_window_->browser()->profile()->GetPrefs();
    return !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
  }

  EncodingMenuController controller;
  if (controller.DoesCommandBelongToEncodingMenu(command_id)) {
    TabContents* tab_contents =
        browser_window_->browser()->GetSelectedTabContents();
    if (tab_contents) {
      return controller.IsItemChecked(browser_window_->browser()->profile(),
                                      tab_contents->encoding(),
                                      command_id);
    }
    return false;
  }

  NOTREACHED();
  return false;
}

void BrowserTitlebar::ExecuteCommandById(int command_id) {
  if (command_id == kShowWindowDecorationsCommand) {
    PrefService* prefs = browser_window_->browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kUseCustomChromeFrame,
                  !prefs->GetBoolean(prefs::kUseCustomChromeFrame));
    return;
  }

  browser_window_->browser()->ExecuteCommand(command_id);
}

void BrowserTitlebar::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::BROWSER_THEME_CHANGED:
      UpdateTextColor();
      break;

    default:
      NOTREACHED();
  }
}

void BrowserTitlebar::ActiveWindowChanged(GdkWindow* active_window) {
  // Can be called during shutdown; BrowserWindowGtk will set our |window_|
  // to NULL during that time.
  if (!window_)
    return;

  window_has_focus_ = GTK_WIDGET(window_)->window == active_window;
  UpdateTextColor();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserTitlebar::Throbber implementation
// TODO(tc): Handle anti-clockwise spinning when waiting for a connection.

// We don't bother to clean up these or the pixbufs they contain when we exit.
static std::vector<GdkPixbuf*>* g_throbber_frames = NULL;
static std::vector<GdkPixbuf*>* g_throbber_waiting_frames = NULL;

// Load |resource_id| from the ResourceBundle and split it into a series of
// square GdkPixbufs that get stored in |frames|.
static void MakeThrobberFrames(int resource_id,
                               std::vector<GdkPixbuf*>* frames) {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  SkBitmap* frame_strip = rb.GetBitmapNamed(resource_id);

  // Each frame of the animation is a square, so we use the height as the
  // frame size.
  int frame_size = frame_strip->height();
  size_t num_frames = frame_strip->width() / frame_size;

  // Make a separate GdkPixbuf for each frame of the animation.
  for (size_t i = 0; i < num_frames; ++i) {
    SkBitmap frame = SkBitmapOperations::CreateTiledBitmap(*frame_strip,
        i * frame_size, 0, frame_size, frame_size);
    frames->push_back(gfx::GdkPixbufFromSkBitmap(&frame));
  }
}

GdkPixbuf* BrowserTitlebar::Throbber::GetNextFrame(bool is_waiting) {
  Throbber::InitFrames();
  if (is_waiting) {
    return (*g_throbber_waiting_frames)[current_waiting_frame_++ %
        g_throbber_waiting_frames->size()];
  } else {
    return (*g_throbber_frames)[current_frame_++ % g_throbber_frames->size()];
  }
}

void BrowserTitlebar::Throbber::Reset() {
  current_frame_ = 0;
  current_waiting_frame_ = 0;
}

// static
void BrowserTitlebar::Throbber::InitFrames() {
  if (g_throbber_frames)
    return;

  // We load the light version of the throbber since it'll be in the titlebar.
  g_throbber_frames = new std::vector<GdkPixbuf*>;
  MakeThrobberFrames(IDR_THROBBER_LIGHT, g_throbber_frames);

  g_throbber_waiting_frames = new std::vector<GdkPixbuf*>;
  MakeThrobberFrames(IDR_THROBBER_WAITING_LIGHT, g_throbber_waiting_frames);
}
