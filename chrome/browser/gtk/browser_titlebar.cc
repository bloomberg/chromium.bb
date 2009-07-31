// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/browser_titlebar.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/gtk_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/standard_menus.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"

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

}  // namespace

BrowserTitlebar::BrowserTitlebar(BrowserWindowGtk* browser_window,
                                 GtkWindow* window)
    : browser_window_(browser_window), window_(window),
      app_mode_favicon_(NULL),
      app_mode_title_(NULL),
      using_custom_frame_(false) {
  Init();
}

void BrowserTitlebar::Init() {
  // The widget hierarchy is shown below.
  //
  // +- EventBox (container_) -------------------------------------------------+
  // +- HBox (container_hbox) -------------------------------------------------+
  // |+- Algn. -++- Alignment --------------++- VBox (titlebar_buttons_box_) -+|
  // ||+ Image +||   (titlebar_alignment_)  ||+- HBox -----------------------+||
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

    // We use the app logo as a placeholder image so the title doesn't jump
    // around.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    // TODO(tc): Add a left click menu to this icon.
    app_mode_favicon_ = gtk_image_new_from_pixbuf(
        rb.GetRTLEnabledPixbufNamed(IDR_PRODUCT_LOGO_16));
    gtk_box_pack_start(GTK_BOX(app_mode_hbox), app_mode_favicon_, FALSE,
                       FALSE, 0);

    app_mode_title_ = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(app_mode_title_), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment(GTK_MISC(app_mode_title_), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(app_mode_hbox), app_mode_title_, TRUE, TRUE,
                       0);
    UpdateTitle();
  }

  // We put the min/max/restore/close buttons in a vbox so they are top aligned
  // and don't vertically stretch.
  titlebar_buttons_box_ = gtk_vbox_new(FALSE, 0);
  GtkWidget* buttons_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(titlebar_buttons_box_), buttons_hbox, FALSE,
                     FALSE, 0);

  close_button_.reset(BuildTitlebarButton(IDR_CLOSE, IDR_CLOSE_P, IDR_CLOSE_H,
                      buttons_hbox, IDS_XPFRAME_CLOSE_TOOLTIP));
  restore_button_.reset(BuildTitlebarButton(IDR_RESTORE, IDR_RESTORE_P,
                        IDR_RESTORE_H, buttons_hbox,
                        IDS_XPFRAME_RESTORE_TOOLTIP));
  maximize_button_.reset(BuildTitlebarButton(IDR_MAXIMIZE, IDR_MAXIMIZE_P,
                         IDR_MAXIMIZE_H, buttons_hbox,
                         IDS_XPFRAME_MAXIMIZE_TOOLTIP));
  minimize_button_.reset(BuildTitlebarButton(IDR_MINIMIZE, IDR_MINIMIZE_P,
                         IDR_MINIMIZE_H, buttons_hbox,
                         IDS_XPFRAME_MINIMIZE_TOOLTIP));

  GtkRequisition req;
  gtk_widget_size_request(close_button_->widget(), &req);
  close_button_default_width_ = req.width;

  gtk_box_pack_end(GTK_BOX(container_hbox), titlebar_buttons_box_, FALSE,
                   FALSE, 0);

  gtk_widget_show_all(container_);
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

void BrowserTitlebar::UpdateTitle() {
  if (!app_mode_title_)
    return;

  // Get the page title and elide it to the available space.
  string16 title = browser_window_->browser()->GetWindowTitleForCurrentTab();

  // TODO(tc): Seems like this color should be themable, but it's hardcoded to
  // white on Windows.  http://crbug.com/18093
  gchar* label_markup = g_markup_printf_escaped("<span color='white'>%s</span>",
      UTF16ToUTF8(title).c_str());
  gtk_label_set_markup(GTK_LABEL(app_mode_title_), label_markup);
  g_free(label_markup);
}

void BrowserTitlebar::UpdateThrobber(bool is_loading) {
  DCHECK(app_mode_favicon_);

  if (is_loading) {
    GdkPixbuf* icon_pixbuf = throbber_.GetNextFrame();
    gtk_image_set_from_pixbuf(GTK_IMAGE(app_mode_favicon_), icon_pixbuf);
  } else {
    if (browser_window_->browser()->type() == Browser::TYPE_APP) {
      SkBitmap icon = browser_window_->browser()->GetCurrentPageIcon();
      if (!icon.empty()) {
        GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
        gtk_image_set_from_pixbuf(GTK_IMAGE(app_mode_favicon_), icon_pixbuf);
        g_object_unref(icon_pixbuf);
      }
    } else {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
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

  int close_button_width = close_button_default_width_;
  if (using_custom_frame_ && browser_window_->IsMaximized())
    close_button_width += kFrameBorderThickness;
  gtk_widget_set_size_request(close_button_->widget(), close_button_width, -1);
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
    gtk_window_unmaximize(titlebar->window_);
  } else if (titlebar->maximize_button_->widget() == button) {
    gtk_window_maximize(titlebar->window_);
  } else if (titlebar->minimize_button_->widget() == button) {
    gtk_window_iconify(titlebar->window_);
  }
}

void BrowserTitlebar::ShowContextMenu() {
  if (!context_menu_.get()) {
    static const MenuCreateMaterial context_menu_blueprint[] = {
        { MENU_NORMAL, IDC_NEW_TAB, IDS_TAB_CXMENU_NEWTAB, 0, NULL,
            GDK_t, GDK_CONTROL_MASK, true },
        { MENU_NORMAL, IDC_RESTORE_TAB, IDS_RESTORE_TAB, 0, NULL,
            GDK_t, GDK_CONTROL_MASK | GDK_SHIFT_MASK, true },
        { MENU_SEPARATOR },
        { MENU_NORMAL, IDC_TASK_MANAGER, IDS_TASK_MANAGER, 0, NULL,
            GDK_Escape, GDK_SHIFT_MASK, true },
        { MENU_SEPARATOR },
        { MENU_CHECKBOX, kShowWindowDecorationsCommand,
            IDS_SHOW_WINDOW_DECORATIONS },
        { MENU_END },
    };

    context_menu_.reset(new MenuGtk(this, context_menu_blueprint, NULL));
  }

  context_menu_->PopupAsContext(gtk_get_current_event_time());
}

bool BrowserTitlebar::IsCommandEnabled(int command_id) const {
  switch (command_id) {
    case IDC_NEW_TAB:
    case kShowWindowDecorationsCommand:
      return true;

    case IDC_RESTORE_TAB:
      return browser_window_->browser()->CanRestoreTab();

    case IDC_TASK_MANAGER:
      return true;

    default:
      NOTREACHED();
  }
  return false;
}

bool BrowserTitlebar::IsItemChecked(int command_id) const {
  DCHECK(command_id == kShowWindowDecorationsCommand);
  PrefService* prefs = browser_window_->browser()->profile()->GetPrefs();
  return !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
}

void BrowserTitlebar::ExecuteCommand(int command_id) {
  switch (command_id) {
    case IDC_NEW_TAB:
    case IDC_RESTORE_TAB:
    case IDC_TASK_MANAGER:
      browser_window_->browser()->ExecuteCommand(command_id);
      break;

    case kShowWindowDecorationsCommand:
    {
      PrefService* prefs = browser_window_->browser()->profile()->GetPrefs();
      prefs->SetBoolean(prefs::kUseCustomChromeFrame,
                        !prefs->GetBoolean(prefs::kUseCustomChromeFrame));
      break;
    }

    default:
      NOTREACHED();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserTitlebar::Throbber implementation
// TODO(tc): Handle anti-clockwise spinning when waiting for a connection.

// We don't bother to clean this or the pixbufs it contains when we exit.
static std::vector<GdkPixbuf*>* g_throbber_frames = NULL;

GdkPixbuf* BrowserTitlebar::Throbber::GetNextFrame() {
  Throbber::InitFrames();
  return (*g_throbber_frames)[current_frame_++ % g_throbber_frames->size()];
}

void BrowserTitlebar::Throbber::Reset() {
  current_frame_ = 0;
}

// static
void BrowserTitlebar::Throbber::InitFrames() {
  if (g_throbber_frames)
    return;

  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  SkBitmap* frame_strip = rb.GetBitmapNamed(IDR_THROBBER_LIGHT);

  // Each frame of the animation is a square, so we use the height as the
  // frame size.
  int frame_size = frame_strip->height();
  size_t num_frames = frame_strip->width() / frame_size;
  g_throbber_frames = new std::vector<GdkPixbuf*>;

  // Make a separate GdkPixbuf for each frame of the animation.
  for (size_t i = 0; i < num_frames; ++i) {
    SkBitmap frame = skia::ImageOperations::CreateTiledBitmap(*frame_strip,
        i * frame_size, 0, frame_size, frame_size);
    g_throbber_frames->push_back(gfx::GdkPixbufFromSkBitmap(&frame));
  }
}
