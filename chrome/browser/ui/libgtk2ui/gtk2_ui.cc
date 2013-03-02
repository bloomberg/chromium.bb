// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"

#include <set>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/nix/mime_util_xdg.h"
#include "base/stl_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/libgtk2ui/chrome_gtk_frame.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"
#include "chrome/browser/ui/libgtk2ui/select_file_dialog_impl.h"
#include "chrome/browser/ui/libgtk2ui/skia_utils_gtk2.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"

// A minimized port of GtkThemeService into something that can provide colors
// and images for aura.
//
// TODO(erg): There's still a lot that needs ported or done for the first time:
//
// - Inject default favicon/folder icons into views somehow.
// - Render and inject the button overlay from the gtk theme.
// - Render and inject the omnibox background.
// - Listen for the "style-set" signal on |fake_frame_| and recreate theme
//   colors and images.
// - Allow not using the theme.
// - Make sure to test with a light on dark theme, too.
// - Everything else that we're not doing.

namespace {

// The size of the rendered toolbar image.
const int kToolbarImageWidth = 64;
const int kToolbarImageHeight = 128;

// How much to tint the GTK+ color lighter at the top of the window.
const color_utils::HSL kGtkFrameShift = { -1, -1, 0.58 };

// How much to tint the GTK+ color when an explicit frame color hasn't been
// specified.
const color_utils::HSL kDefaultFrameShift = { -1, -1, 0.4 };

// Values used as the new luminance and saturation values in the inactive tab
// text color.
const double kDarkInactiveLuminance = 0.85;
const double kLightInactiveLuminance = 0.15;
const double kHeavyInactiveSaturation = 0.7;
const double kLightInactiveSaturation = 0.3;

// Default color for links on the NTP when the GTK+ theme doesn't define a
// link color. Constant taken from gtklinkbutton.c.
const GdkColor kDefaultLinkColor = { 0, 0, 0, 0xeeee };

const int kSkiaToGDKMultiplier = 257;


// TODO(erg): ThemeService has a whole interface just for reading default
// constants. Figure out what to do with that more long term; for now, just
// copy the constants themselves here.
//
// Default tints.
const color_utils::HSL kDefaultTintButtons = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrame = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrameInactive = { -1, -1, 0.75f };
const color_utils::HSL kDefaultTintFrameIncognito = { -1, 0.2f, 0.35f };
const color_utils::HSL kDefaultTintFrameIncognitoInactive = { -1, 0.3f, 0.6f };
const color_utils::HSL kDefaultTintBackgroundTab = { -1, 0.5, 0.75 };

// A list of images that we provide while in gtk mode.
const int kThemeImages[] = {
  IDR_THEME_TOOLBAR,
  IDR_THEME_TAB_BACKGROUND,
  IDR_THEME_TAB_BACKGROUND_INCOGNITO,
  IDR_THEME_FRAME,
  IDR_THEME_FRAME_INACTIVE,
  IDR_THEME_FRAME_INCOGNITO,
  IDR_THEME_FRAME_INCOGNITO_INACTIVE,
};

// A list of icons used in the autocomplete view that should be tinted to the
// current gtk theme selection color so they stand out against the GtkEntry's
// base color.
const int kAutocompleteImages[] = {
  IDR_OMNIBOX_EXTENSION_APP,
  IDR_OMNIBOX_HTTP,
  IDR_OMNIBOX_HTTP_DARK,
  IDR_OMNIBOX_SEARCH,
  IDR_OMNIBOX_SEARCH_DARK,
  IDR_OMNIBOX_STAR,
  IDR_OMNIBOX_STAR_DARK,
  IDR_OMNIBOX_TTS,
  IDR_OMNIBOX_TTS_DARK,
  IDR_GEOLOCATION_ALLOWED_LOCATIONBAR_ICON,
  IDR_GEOLOCATION_DENIED_LOCATIONBAR_ICON,
  IDR_REGISTER_PROTOCOL_HANDLER_LOCATIONBAR_ICON,
};

// This table converts button ids into a pair of gtk-stock id and state.
struct IDRGtkMapping {
  int idr;
  const char* stock_id;
  GtkStateType gtk_state;
} const kGtkIcons[] = {
  { IDR_BACK,      GTK_STOCK_GO_BACK,    GTK_STATE_NORMAL },
  { IDR_BACK_D,    GTK_STOCK_GO_BACK,    GTK_STATE_INSENSITIVE },
  { IDR_BACK_H,    GTK_STOCK_GO_BACK,    GTK_STATE_PRELIGHT },
  { IDR_BACK_P,    GTK_STOCK_GO_BACK,    GTK_STATE_ACTIVE },

  { IDR_FORWARD,   GTK_STOCK_GO_FORWARD, GTK_STATE_NORMAL },
  { IDR_FORWARD_D, GTK_STOCK_GO_FORWARD, GTK_STATE_INSENSITIVE },
  { IDR_FORWARD_H, GTK_STOCK_GO_FORWARD, GTK_STATE_PRELIGHT },
  { IDR_FORWARD_P, GTK_STOCK_GO_FORWARD, GTK_STATE_ACTIVE },

  { IDR_HOME,      GTK_STOCK_HOME,       GTK_STATE_NORMAL },
  { IDR_HOME_H,    GTK_STOCK_HOME,       GTK_STATE_PRELIGHT },
  { IDR_HOME_P,    GTK_STOCK_HOME,       GTK_STATE_ACTIVE },

  { IDR_RELOAD,    GTK_STOCK_REFRESH,    GTK_STATE_NORMAL },
  { IDR_RELOAD_H,  GTK_STOCK_REFRESH,    GTK_STATE_PRELIGHT },
  { IDR_RELOAD_P,  GTK_STOCK_REFRESH,    GTK_STATE_ACTIVE },

  { IDR_STOP,      GTK_STOCK_STOP,       GTK_STATE_NORMAL },
  { IDR_STOP_D,    GTK_STOCK_STOP,       GTK_STATE_INSENSITIVE },
  { IDR_STOP_H,    GTK_STOCK_STOP,       GTK_STATE_PRELIGHT },
  { IDR_STOP_P,    GTK_STOCK_STOP,       GTK_STATE_ACTIVE },
};

// The image resources that will be tinted by the 'button' tint value.
const int kOtherToolbarButtonIDs[] = {
  IDR_TOOLS, IDR_TOOLS_H, IDR_TOOLS_P,

  // TODO(erg): The rest of these need to have some sort of injection done.
  IDR_LOCATIONBG_C, IDR_LOCATIONBG_L, IDR_LOCATIONBG_R,
  IDR_BROWSER_ACTIONS_OVERFLOW, IDR_BROWSER_ACTIONS_OVERFLOW_H,
  IDR_BROWSER_ACTIONS_OVERFLOW_P,
  IDR_MENU_DROPARROW,
  IDR_THROBBER, IDR_THROBBER_WAITING, IDR_THROBBER_LIGHT,
};

bool IsOverridableImage(int id) {
  CR_DEFINE_STATIC_LOCAL(std::set<int>, images, ());
  if (images.empty()) {
    images.insert(kThemeImages, kThemeImages + arraysize(kThemeImages));
    images.insert(kAutocompleteImages,
                  kAutocompleteImages + arraysize(kAutocompleteImages));

    for (unsigned int i = 0; i < arraysize(kGtkIcons); ++i)
      images.insert(kGtkIcons[i].idr);

    images.insert(kOtherToolbarButtonIDs,
                  kOtherToolbarButtonIDs + arraysize(kOtherToolbarButtonIDs));
  }

  return images.count(id) > 0;
}

// Picks a button tint from a set of background colors. While
// |accent_gdk_color| will usually be the same color through a theme, this
// function will get called with the normal GtkLabel |text_color|/GtkWindow
// |background_color| pair and the GtkEntry |text_color|/|background_color|
// pair. While 3/4 of the time the resulting tint will be the same, themes that
// have a dark window background (with light text) and a light text entry (with
// dark text) will get better icons with this separated out.
void PickButtonTintFromColors(const GdkColor& accent_gdk_color,
                              const GdkColor& text_color,
                              const GdkColor& background_color,
                              color_utils::HSL* tint) {
  SkColor accent_color = libgtk2ui::GdkColorToSkColor(accent_gdk_color);
  color_utils::HSL accent_tint;
  color_utils::SkColorToHSL(accent_color, &accent_tint);

  color_utils::HSL text_tint;
  color_utils::SkColorToHSL(libgtk2ui::GdkColorToSkColor(text_color),
                            &text_tint);

  color_utils::HSL background_tint;
  color_utils::SkColorToHSL(libgtk2ui::GdkColorToSkColor(background_color),
                            &background_tint);

  // If the accent color is gray, then our normal HSL tomfoolery will bring out
  // whatever color is oddly dominant (for example, in rgb space [125, 128,
  // 125] will tint green instead of gray). Slight differences (+/-10 (4%) to
  // all color components) should be interpreted as this color being gray and
  // we should switch into a special grayscale mode.
  int rb_diff = abs(SkColorGetR(accent_color) - SkColorGetB(accent_color));
  int rg_diff = abs(SkColorGetR(accent_color) - SkColorGetG(accent_color));
  int bg_diff = abs(SkColorGetB(accent_color) - SkColorGetG(accent_color));
  if (rb_diff < 10 && rg_diff < 10 && bg_diff < 10) {
    // Our accent is white/gray/black. Only the luminance of the accent color
    // matters.
    tint->h = -1;

    // Use the saturation of the text.
    tint->s = text_tint.s;

    // Use the luminance of the accent color UNLESS there isn't enough
    // luminance contrast between the accent color and the base color.
    if (fabs(accent_tint.l - background_tint.l) > 0.3)
      tint->l = accent_tint.l;
    else
      tint->l = text_tint.l;
  } else {
    // Our accent is a color.
    tint->h = accent_tint.h;

    // Don't modify the saturation; the amount of color doesn't matter.
    tint->s = -1;

    // If the text wants us to darken the icon, don't change the luminance (the
    // icons are already dark enough). Otherwise, lighten the icon by no more
    // than 0.9 since we don't want a pure-white icon even if the text is pure
    // white.
    if (text_tint.l < 0.5)
      tint->l = -1;
    else if (text_tint.l <= 0.9)
      tint->l = text_tint.l;
    else
      tint->l = 0.9;
  }
}

// Applies an HSL shift to a GdkColor (instead of an SkColor)
void GdkColorHSLShift(const color_utils::HSL& shift, GdkColor* frame_color) {
  SkColor shifted = color_utils::HSLShift(
      libgtk2ui::GdkColorToSkColor(*frame_color), shift);

  frame_color->pixel = 0;
  frame_color->red = SkColorGetR(shifted) * kSkiaToGDKMultiplier;
  frame_color->green = SkColorGetG(shifted) * kSkiaToGDKMultiplier;
  frame_color->blue = SkColorGetB(shifted) * kSkiaToGDKMultiplier;
}

// Copied Default blah sections from ThemeService.
color_utils::HSL GetDefaultTint(int id) {
  switch (id) {
    case ThemeProperties::TINT_FRAME:
      return kDefaultTintFrame;
    case ThemeProperties::TINT_FRAME_INACTIVE:
      return kDefaultTintFrameInactive;
    case ThemeProperties::TINT_FRAME_INCOGNITO:
      return kDefaultTintFrameIncognito;
    case ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE:
      return kDefaultTintFrameIncognitoInactive;
    case ThemeProperties::TINT_BUTTONS:
      return kDefaultTintButtons;
    case ThemeProperties::TINT_BACKGROUND_TAB:
      return kDefaultTintBackgroundTab;
    default:
      color_utils::HSL result = {-1, -1, -1};
      return result;
  }
}

}  // namespace

namespace libgtk2ui {

Gtk2UI::Gtk2UI() {
  DLOG(ERROR) << "Activating the gtk2 component";
  GtkInitFromCommandLine(*CommandLine::ForCurrentProcess());

  // Create our fake widgets.
  fake_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  fake_frame_ = chrome_gtk_frame_new();
  fake_label_.Own(gtk_label_new(""));
  fake_entry_.Own(gtk_entry_new());

  // Only realized widgets receive style-set notifications, which we need to
  // broadcast new theme images and colors. Only realized widgets have style
  // properties, too, which we query for some colors.
  gtk_widget_realize(fake_frame_);
  gtk_widget_realize(fake_window_);
  // TODO: Also listen for "style-set" on the fake frame.

  // TODO(erg): Be lazy about generating this data and connect it to the
  // style-set signal handler.
  LoadGtkValues();
  SetXDGIconTheme();
}

Gtk2UI::~Gtk2UI() {
  gtk_widget_destroy(fake_window_);
  gtk_widget_destroy(fake_frame_);
  fake_label_.Destroy();
  fake_entry_.Destroy();

  ClearAllThemeData();
}

bool Gtk2UI::UseNativeTheme() const {
  return true;
}

gfx::Image* Gtk2UI::GetThemeImageNamed(int id) const {
  // Try to get our cached version:
  ImageCache::const_iterator it = gtk_images_.find(id);
  if (it != gtk_images_.end())
    return it->second;

  if (/*use_gtk_ && */ IsOverridableImage(id)) {
    gfx::Image* image = new gfx::Image(
        gfx::ImageSkia::CreateFrom1xBitmap(GenerateGtkThemeBitmap(id)));
    gtk_images_[id] = image;
    return image;
  }

  return NULL;
}

bool Gtk2UI::GetColor(int id, SkColor* color) const {
  ColorMap::const_iterator it = colors_.find(id);
  if (it != colors_.end()) {
    *color = it->second;
    return true;
  }

  return false;
}

ui::NativeTheme* Gtk2UI::GetNativeTheme() const {
  return NULL;
}

ui::SelectFileDialog* Gtk2UI::CreateSelectFileDialog(
    ui::SelectFileDialog::Listener* listener,
    ui::SelectFilePolicy* policy) const {
  return SelectFileDialogImpl::Create(listener, policy);
}

void Gtk2UI::GetScrollbarColors(GdkColor* thumb_active_color,
                                GdkColor* thumb_inactive_color,
                                GdkColor* track_color) {
  const GdkColor* theme_thumb_active = NULL;
  const GdkColor* theme_thumb_inactive = NULL;
  const GdkColor* theme_trough_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_frame_),
                       "scrollbar-slider-prelight-color", &theme_thumb_active,
                       "scrollbar-slider-normal-color", &theme_thumb_inactive,
                       "scrollbar-trough-color", &theme_trough_color,
                       NULL);

  // Ask the theme if the theme specifies all the scrollbar colors and short
  // circuit the expensive painting/compositing if we have all of them.
  if (theme_thumb_active && theme_thumb_inactive && theme_trough_color) {
    *thumb_active_color = *theme_thumb_active;
    *thumb_inactive_color = *theme_thumb_inactive;
    *track_color = *theme_trough_color;
    return;
  }

  // Create window containing scrollbar elements
  GtkWidget* window    = gtk_window_new(GTK_WINDOW_POPUP);
  GtkWidget* fixed     = gtk_fixed_new();
  GtkWidget* scrollbar = gtk_hscrollbar_new(NULL);
  gtk_container_add(GTK_CONTAINER(window), fixed);
  gtk_container_add(GTK_CONTAINER(fixed),  scrollbar);
  gtk_widget_realize(window);
  gtk_widget_realize(scrollbar);

  // Draw scrollbar thumb part and track into offscreen image
  const int kWidth  = 100;
  const int kHeight = 20;
  GtkStyle*  style  = gtk_rc_get_style(scrollbar);
  GdkWindow* gdk_window = gtk_widget_get_window(window);
  GdkPixmap* pm     = gdk_pixmap_new(gdk_window, kWidth, kHeight, -1);
  GdkRectangle rect = { 0, 0, kWidth, kHeight };
  unsigned char data[3 * kWidth * kHeight];
  for (int i = 0; i < 3; ++i) {
    if (i < 2) {
      // Thumb part
      gtk_paint_slider(style, pm,
                       i == 0 ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL,
                       GTK_SHADOW_OUT, &rect, scrollbar, "slider", 0, 0,
                       kWidth, kHeight, GTK_ORIENTATION_HORIZONTAL);
    } else {
      // Track
      gtk_paint_box(style, pm, GTK_STATE_ACTIVE, GTK_SHADOW_IN, &rect,
                    scrollbar, "trough-upper", 0, 0, kWidth, kHeight);
    }
    GdkPixbuf* pb = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB,
                                             FALSE, 8, kWidth, kHeight,
                                             3 * kWidth, 0, 0);
    gdk_pixbuf_get_from_drawable(pb, pm, NULL, 0, 0, 0, 0, kWidth, kHeight);

    // Sample pixels
    int components[3] = { 0 };
    for (int y = 2; y < kHeight - 2; ++y) {
      for (int c = 0; c < 3; ++c) {
        // Sample a vertical slice of pixels at about one-thirds from the
        // left edge. This allows us to avoid any fixed graphics that might be
        // located at the edges or in the center of the scrollbar.
        // Each pixel is made up of a red, green, and blue component; taking up
        // a total of three bytes.
        components[c] += data[3 * (kWidth / 3 + y * kWidth) + c];
      }
    }
    GdkColor* color = i == 0 ? thumb_active_color :
                      i == 1 ? thumb_inactive_color :
                               track_color;
    color->pixel = 0;
    // We sampled pixels across the full height of the image, ignoring a two
    // pixel border. In some themes, the border has a completely different
    // color which we do not want to factor into our average color computation.
    //
    // We now need to scale the colors from the 0..255 range, to the wider
    // 0..65535 range, and we need to actually compute the average color; so,
    // we divide by the total number of pixels in the sample.
    color->red   = components[0] * 65535 / (255 * (kHeight - 4));
    color->green = components[1] * 65535 / (255 * (kHeight - 4));
    color->blue  = components[2] * 65535 / (255 * (kHeight - 4));

    g_object_unref(pb);
  }
  g_object_unref(pm);

  gtk_widget_destroy(window);

  // Override any of the default colors with ones that were specified by the
  // theme.
  if (theme_thumb_active)
    *thumb_active_color = *theme_thumb_active;

  if (theme_thumb_inactive)
    *thumb_inactive_color = *theme_thumb_inactive;

  if (theme_trough_color)
    *track_color = *theme_trough_color;
}

void Gtk2UI::SetXDGIconTheme() {
  gchar* gtk_theme_name;
  g_object_get(gtk_settings_get_default(),
               "gtk-icon-theme-name",
               &gtk_theme_name, NULL);
  base::nix::SetIconThemeName(gtk_theme_name);
  g_free(gtk_theme_name);
}

void Gtk2UI::LoadGtkValues() {
  // TODO(erg): GtkThemeService had a comment here about having to muck with
  // the raw Prefs object to remove prefs::kCurrentThemeImages or else we'd
  // regress startup time. Figure out how to do that when we can't access the
  // prefs system from here.

  GtkStyle* frame_style = gtk_rc_get_style(fake_frame_);

  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  SetThemeColorFromGtk(ThemeProperties::COLOR_CONTROL_BACKGROUND,
                       &window_style->bg[GTK_STATE_NORMAL]);

  GdkColor toolbar_color = window_style->bg[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeProperties::COLOR_TOOLBAR, &toolbar_color);

  GdkColor button_color = window_style->bg[GTK_STATE_SELECTED];
  SetThemeTintFromGtk(ThemeProperties::TINT_BUTTONS, &button_color);

  GtkStyle* label_style = gtk_rc_get_style(fake_label_.get());
  GdkColor label_color = label_style->fg[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeProperties::COLOR_TAB_TEXT, &label_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_BOOKMARK_TEXT, &label_color);

  // Build the various icon tints.
  GetNormalButtonTintHSL(&button_tint_);
  GetNormalEntryForegroundHSL(&entry_tint_);
  GetSelectedEntryForegroundHSL(&selected_entry_tint_);
  GdkColor frame_color = BuildFrameColors(frame_style);

  // The inactive frame color never occurs naturally in the theme, as it is a
  // tinted version of |frame_color|. We generate another color based on the
  // background tab color, with the lightness and saturation moved in the
  // opposite direction. (We don't touch the hue, since there should be subtle
  // hints of the color in the text.)
  color_utils::HSL inactive_tab_text_hsl =
      tints_[ThemeProperties::TINT_BACKGROUND_TAB];
  if (inactive_tab_text_hsl.l < 0.5)
    inactive_tab_text_hsl.l = kDarkInactiveLuminance;
  else
    inactive_tab_text_hsl.l = kLightInactiveLuminance;

  if (inactive_tab_text_hsl.s < 0.5)
    inactive_tab_text_hsl.s = kHeavyInactiveSaturation;
  else
    inactive_tab_text_hsl.s = kLightInactiveSaturation;

  colors_[ThemeProperties::COLOR_BACKGROUND_TAB_TEXT] =
      color_utils::HSLToSkColor(inactive_tab_text_hsl, 255);

  // We pick the text and background colors for the NTP out of the colors for a
  // GtkEntry. We do this because GtkEntries background color is never the same
  // as |toolbar_color|, is usually a white, and when it isn't a white,
  // provides sufficient contrast to |toolbar_color|. Try this out with
  // Darklooks, HighContrastInverse or ThinIce.
  GtkStyle* entry_style = gtk_rc_get_style(fake_entry_.get());
  GdkColor ntp_background = entry_style->base[GTK_STATE_NORMAL];
  GdkColor ntp_foreground = entry_style->text[GTK_STATE_NORMAL];
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_BACKGROUND,
                       &ntp_background);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_TEXT,
                       &ntp_foreground);

  // The NTP header is the color that surrounds the current active thumbnail on
  // the NTP, and acts as the border of the "Recent Links" box. It would be
  // awesome if they were separated so we could use GetBorderColor() for the
  // border around the "Recent Links" section, but matching the frame color is
  // more important.
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_HEADER,
                       &frame_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_SECTION,
                       &toolbar_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_SECTION_TEXT,
                       &label_color);

  // Override the link color if the theme provides it.
  const GdkColor* link_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_window_),
                       "link-color", &link_color, NULL);

  bool is_default_link_color = false;
  if (!link_color) {
    link_color = &kDefaultLinkColor;
    is_default_link_color = true;
  }

  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_LINK,
                       link_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_LINK_UNDERLINE,
                       link_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_SECTION_LINK,
                       link_color);
  SetThemeColorFromGtk(ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE,
                       link_color);

  if (!is_default_link_color)
    gdk_color_free(const_cast<GdkColor*>(link_color));

  // Generate the colors that we pass to WebKit.
  focus_ring_color_ = GdkColorToSkColor(frame_color);

  GdkColor thumb_active_color, thumb_inactive_color, track_color;
  Gtk2UI::GetScrollbarColors(&thumb_active_color,
                             &thumb_inactive_color,
                             &track_color);
  thumb_active_color_ = GdkColorToSkColor(thumb_active_color);
  thumb_inactive_color_ = GdkColorToSkColor(thumb_inactive_color);
  track_color_ = GdkColorToSkColor(track_color);

  // Some GTK themes only define the text selection colors on the GtkEntry
  // class, so we need to use that for getting selection colors.
  active_selection_bg_color_ =
      GdkColorToSkColor(entry_style->base[GTK_STATE_SELECTED]);
  active_selection_fg_color_ =
      GdkColorToSkColor(entry_style->text[GTK_STATE_SELECTED]);
  inactive_selection_bg_color_ =
      GdkColorToSkColor(entry_style->base[GTK_STATE_ACTIVE]);
  inactive_selection_fg_color_ =
      GdkColorToSkColor(entry_style->text[GTK_STATE_ACTIVE]);
}

GdkColor Gtk2UI::BuildFrameColors(GtkStyle* frame_style) {
  GdkColor* theme_frame = NULL;
  GdkColor* theme_inactive_frame = NULL;
  GdkColor* theme_incognito_frame = NULL;
  GdkColor* theme_incognito_inactive_frame = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_frame_),
                       "frame-color", &theme_frame,
                       "inactive-frame-color", &theme_inactive_frame,
                       "incognito-frame-color", &theme_incognito_frame,
                       "incognito-inactive-frame-color",
                       &theme_incognito_inactive_frame,
                       NULL);

  GdkColor frame_color = BuildAndSetFrameColor(
      &frame_style->bg[GTK_STATE_SELECTED],
      theme_frame,
      kDefaultFrameShift,
      ThemeProperties::COLOR_FRAME,
      ThemeProperties::TINT_FRAME);
  if (theme_frame)
    gdk_color_free(theme_frame);
  SetThemeTintFromGtk(ThemeProperties::TINT_BACKGROUND_TAB, &frame_color);

  BuildAndSetFrameColor(
      &frame_style->bg[GTK_STATE_INSENSITIVE],
      theme_inactive_frame,
      kDefaultFrameShift,
      ThemeProperties::COLOR_FRAME_INACTIVE,
      ThemeProperties::TINT_FRAME_INACTIVE);
  if (theme_inactive_frame)
    gdk_color_free(theme_inactive_frame);

  BuildAndSetFrameColor(
      &frame_color,
      theme_incognito_frame,
      GetDefaultTint(ThemeProperties::TINT_FRAME_INCOGNITO),
      ThemeProperties::COLOR_FRAME_INCOGNITO,
      ThemeProperties::TINT_FRAME_INCOGNITO);
  if (theme_incognito_frame)
    gdk_color_free(theme_incognito_frame);

  BuildAndSetFrameColor(
      &frame_color,
      theme_incognito_inactive_frame,
      GetDefaultTint(ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE),
      ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE,
      ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE);
  if (theme_incognito_inactive_frame)
    gdk_color_free(theme_incognito_inactive_frame);

  return frame_color;
}

void Gtk2UI::SetThemeColorFromGtk(int id, const GdkColor* color) {
  colors_[id] = GdkColorToSkColor(*color);
}

void Gtk2UI::SetThemeTintFromGtk(int id, const GdkColor* color) {
  color_utils::HSL default_tint = GetDefaultTint(id);
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(GdkColorToSkColor(*color), &hsl);

  if (default_tint.s != -1)
    hsl.s = default_tint.s;

  if (default_tint.l != -1)
    hsl.l = default_tint.l;

  tints_[id] = hsl;
}

GdkColor Gtk2UI::BuildAndSetFrameColor(const GdkColor* base,
                                       const GdkColor* gtk_base,
                                       const color_utils::HSL& tint,
                                       int color_id,
                                       int tint_id) {
  GdkColor out_color = *base;
  if (gtk_base) {
    // The theme author specified a color to use, use it without modification.
    out_color = *gtk_base;
  } else {
    // Tint the basic color since this is a heuristic color instead of one
    // specified by the theme author.
    GdkColorHSLShift(tint, &out_color);
  }
  SetThemeColorFromGtk(color_id, &out_color);
  SetThemeTintFromGtk(tint_id, &out_color);

  return out_color;
}

SkBitmap Gtk2UI::GenerateGtkThemeBitmap(int id) const {
  switch (id) {
    case IDR_THEME_TOOLBAR: {
      GtkStyle* style = gtk_rc_get_style(fake_window_);
      GdkColor* color = &style->bg[GTK_STATE_NORMAL];
      SkBitmap bitmap;
      bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                       kToolbarImageWidth, kToolbarImageHeight);
      bitmap.allocPixels();
      bitmap.eraseRGB(color->red >> 8, color->green >> 8, color->blue >> 8);
      return bitmap;
    }
    case IDR_THEME_TAB_BACKGROUND:
      return GenerateTabImage(IDR_THEME_FRAME);
    case IDR_THEME_TAB_BACKGROUND_INCOGNITO:
      return GenerateTabImage(IDR_THEME_FRAME_INCOGNITO);
    case IDR_THEME_FRAME:
      return GenerateFrameImage(ThemeProperties::COLOR_FRAME,
                                "frame-gradient-color");
    case IDR_THEME_FRAME_INACTIVE:
      return GenerateFrameImage(ThemeProperties::COLOR_FRAME_INACTIVE,
                                "inactive-frame-gradient-color");
    case IDR_THEME_FRAME_INCOGNITO:
      return GenerateFrameImage(ThemeProperties::COLOR_FRAME_INCOGNITO,
                                "incognito-frame-gradient-color");
    case IDR_THEME_FRAME_INCOGNITO_INACTIVE: {
      return GenerateFrameImage(
          ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE,
          "incognito-inactive-frame-gradient-color");
    }
    // Icons that sit inside the omnibox shouldn't receive TINT_BUTTONS and
    // instead should tint based on the foreground text entry color in GTK+
    // mode because some themes that try to be dark *and* light have very
    // different colors between the omnibox and the normal background area.
    case IDR_OMNIBOX_EXTENSION_APP:
    case IDR_OMNIBOX_HTTP:
    case IDR_OMNIBOX_SEARCH:
    case IDR_OMNIBOX_STAR:
    case IDR_OMNIBOX_TTS:
    case IDR_GEOLOCATION_ALLOWED_LOCATIONBAR_ICON:
    case IDR_GEOLOCATION_DENIED_LOCATIONBAR_ICON:
    case IDR_REGISTER_PROTOCOL_HANDLER_LOCATIONBAR_ICON: {
      return GenerateTintedIcon(id, entry_tint_);
    }
    // In GTK mode, the dark versions of the omnibox icons only ever appear in
    // the autocomplete popup and only against the current theme's GtkEntry
    // base[GTK_STATE_SELECTED] color, so tint the icons so they won't collide
    // with the selected color.
    case IDR_OMNIBOX_EXTENSION_APP_DARK:
    case IDR_OMNIBOX_HTTP_DARK:
    case IDR_OMNIBOX_SEARCH_DARK:
    case IDR_OMNIBOX_STAR_DARK:
    case IDR_OMNIBOX_TTS_DARK: {
      return GenerateTintedIcon(id, selected_entry_tint_);
    }
    // In GTK mode, we need to manually render several icons.
    case IDR_BACK:
    case IDR_BACK_D:
    case IDR_BACK_H:
    case IDR_BACK_P:
    case IDR_FORWARD:
    case IDR_FORWARD_D:
    case IDR_FORWARD_H:
    case IDR_FORWARD_P:
    case IDR_HOME:
    case IDR_HOME_H:
    case IDR_HOME_P:
    case IDR_RELOAD:
    case IDR_RELOAD_H:
    case IDR_RELOAD_P:
    case IDR_STOP:
    case IDR_STOP_D:
    case IDR_STOP_H:
    case IDR_STOP_P: {
      return GenerateGTKIcon(id);
    }
    case IDR_TOOLS:
    case IDR_TOOLS_H:
    case IDR_TOOLS_P: {
      return GenerateWrenchIcon(id);
    }
    default: {
      return GenerateTintedIcon(id, button_tint_);
    }
  }

  return SkBitmap();
}

SkBitmap Gtk2UI::GenerateFrameImage(
    int color_id,
    const char* gradient_name) const {
  // We use two colors: the main color (passed in) and a lightened version of
  // that color (which is supposed to match the light gradient at the top of
  // several GTK+ themes, such as Ambiance, Clearlooks or Bluebird).
  ColorMap::const_iterator it = colors_.find(color_id);
  DCHECK(it != colors_.end());
  SkColor base = it->second;

  gfx::Canvas canvas(gfx::Size(kToolbarImageWidth, kToolbarImageHeight),
      ui::SCALE_FACTOR_100P, true);

  int gradient_size;
  GdkColor* gradient_top_color = NULL;
  gtk_widget_style_get(GTK_WIDGET(fake_frame_),
                       "frame-gradient-size", &gradient_size,
                       gradient_name, &gradient_top_color,
                       NULL);
  if (gradient_size) {
    SkColor lighter = gradient_top_color ?
        GdkColorToSkColor(*gradient_top_color) :
        color_utils::HSLShift(base, kGtkFrameShift);
    if (gradient_top_color)
      gdk_color_free(gradient_top_color);
    skia::RefPtr<SkShader> shader = gfx::CreateGradientShader(
        0, gradient_size, lighter, base);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setShader(shader.get());

    canvas.DrawRect(gfx::Rect(0, 0, kToolbarImageWidth, gradient_size), paint);
  }

  canvas.FillRect(gfx::Rect(0, gradient_size, kToolbarImageWidth,
                            kToolbarImageHeight - gradient_size), base);
  return canvas.ExtractImageRep().sk_bitmap();
}

SkBitmap Gtk2UI::GenerateTabImage(int base_id) const {
  const SkBitmap* base_image = GetThemeImageNamed(base_id)->ToSkBitmap();
  SkBitmap bg_tint = SkBitmapOperations::CreateHSLShiftedBitmap(
      *base_image, GetDefaultTint(ThemeProperties::TINT_BACKGROUND_TAB));
  return SkBitmapOperations::CreateTiledBitmap(
      bg_tint, 0, 0, bg_tint.width(), bg_tint.height());
}

SkBitmap Gtk2UI::GenerateTintedIcon(
    int base_id,
    const color_utils::HSL& tint) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return SkBitmapOperations::CreateHSLShiftedBitmap(
      rb.GetImageNamed(base_id).AsBitmap(), tint);
}

SkBitmap Gtk2UI::GenerateGTKIcon(int base_id) const {
  const char* stock_id = NULL;
  GtkStateType gtk_state = GTK_STATE_NORMAL;
  for (unsigned int i = 0; i < arraysize(kGtkIcons); ++i) {
    if (kGtkIcons[i].idr == base_id) {
      stock_id = kGtkIcons[i].stock_id;
      gtk_state = kGtkIcons[i].gtk_state;
      break;
    }
  }
  DCHECK(stock_id);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SkBitmap default_bitmap = rb.GetImageNamed(base_id).AsBitmap();

  gtk_widget_ensure_style(fake_frame_);
  GtkStyle* style = gtk_widget_get_style(fake_frame_);
  GtkIconSet* icon_set = gtk_style_lookup_icon_set(style, stock_id);
  if (!icon_set)
    return default_bitmap;

  // Ask GTK to render the icon to a buffer, which we will steal from.
  GdkPixbuf* gdk_icon = gtk_icon_set_render_icon(
      icon_set,
      style,
      base::i18n::IsRTL() ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR,
      gtk_state,
      GTK_ICON_SIZE_SMALL_TOOLBAR,
      fake_frame_,
      NULL);

  if (!gdk_icon) {
    // This can theoretically happen if an icon theme doesn't provide a
    // specific image. This should realistically never happen, but I bet there
    // are some theme authors who don't reliably provide all icons.
    return default_bitmap;
  }

  SkBitmap retval;
  retval.setConfig(SkBitmap::kARGB_8888_Config,
                   default_bitmap.width(),
                   default_bitmap.height());
  retval.allocPixels();
  retval.eraseColor(0);

  const SkBitmap icon = GdkPixbufToImageSkia(gdk_icon);
  g_object_unref(gdk_icon);

  SkCanvas canvas(retval);

  if (gtk_state == GTK_STATE_ACTIVE || gtk_state == GTK_STATE_PRELIGHT) {
    SkBitmap border = DrawGtkButtonBorder(gtk_state,
                                          default_bitmap.width(),
                                          default_bitmap.height());
    canvas.drawBitmap(border, 0, 0);
  }

  canvas.drawBitmap(icon,
                    (default_bitmap.width() / 2) - (icon.width() / 2),
                    (default_bitmap.height() / 2) - (icon.height() / 2));

  return retval;
}

SkBitmap Gtk2UI::GenerateWrenchIcon(int base_id) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SkBitmap default_bitmap = rb.GetImageNamed(IDR_TOOLS).AsBitmap();

  // Part 1: Tint the wrench icon according to the button tint.
  SkBitmap shifted = SkBitmapOperations::CreateHSLShiftedBitmap(
      default_bitmap, button_tint_);

  // The unhighlighted icon doesn't need to have a border composed onto it.
  if (base_id == IDR_TOOLS)
    return shifted;

  SkBitmap retval;
  retval.setConfig(SkBitmap::kARGB_8888_Config,
                   default_bitmap.width(),
                   default_bitmap.height());
  retval.allocPixels();
  retval.eraseColor(0);

  SkCanvas canvas(retval);
  SkBitmap border = DrawGtkButtonBorder(
      base_id == IDR_TOOLS_H ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE,
      default_bitmap.width(),
      default_bitmap.height());
  canvas.drawBitmap(border, 0, 0);
  canvas.drawBitmap(shifted, 0, 0);

  return retval;
}

void Gtk2UI::GetNormalButtonTintHSL(color_utils::HSL* tint) const {
  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  const GdkColor accent_gdk_color = window_style->bg[GTK_STATE_SELECTED];
  const GdkColor base_color = window_style->base[GTK_STATE_NORMAL];

  GtkStyle* label_style = gtk_rc_get_style(fake_label_.get());
  const GdkColor text_color = label_style->fg[GTK_STATE_NORMAL];

  PickButtonTintFromColors(accent_gdk_color, text_color, base_color, tint);
}

void Gtk2UI::GetNormalEntryForegroundHSL(color_utils::HSL* tint) const {
  GtkStyle* window_style = gtk_rc_get_style(fake_window_);
  const GdkColor accent_gdk_color = window_style->bg[GTK_STATE_SELECTED];

  GtkStyle* style = gtk_rc_get_style(fake_entry_.get());
  const GdkColor text_color = style->text[GTK_STATE_NORMAL];
  const GdkColor base_color = style->base[GTK_STATE_NORMAL];

  PickButtonTintFromColors(accent_gdk_color, text_color, base_color, tint);
}

void Gtk2UI::GetSelectedEntryForegroundHSL(color_utils::HSL* tint) const {
  // The simplest of all the tints. We just use the selected text in the entry
  // since the icons tinted this way will only be displayed against
  // base[GTK_STATE_SELECTED].
  GtkStyle* style = gtk_rc_get_style(fake_entry_.get());
  const GdkColor color = style->text[GTK_STATE_SELECTED];
  color_utils::SkColorToHSL(GdkColorToSkColor(color), tint);
}

SkBitmap Gtk2UI::DrawGtkButtonBorder(int gtk_state,
                                     int width, int height) const {
  // Create a temporary GTK button to snapshot
  GtkWidget* window = gtk_offscreen_window_new();
  GtkWidget* button = gtk_button_new();
  gtk_widget_set_size_request(button, width, height);
  gtk_container_add(GTK_CONTAINER(window), button);
  gtk_widget_realize(window);
  gtk_widget_realize(button);
  gtk_widget_show(button);
  gtk_widget_show(window);

  gtk_widget_set_state(button, static_cast<GtkStateType>(gtk_state));

  GdkPixmap* pixmap = gtk_widget_get_snapshot(button, NULL);
  int w, h;
  gdk_drawable_get_size(GDK_DRAWABLE(pixmap), &w, &h);
  DCHECK_EQ(w, width);
  DCHECK_EQ(h, height);

  // We render the Pixmap to a Pixbuf. This can be slow, as we're scrapping
  // bits from X.
  GdkColormap* colormap = gdk_drawable_get_colormap(pixmap);
  GdkPixbuf* pixbuf = gdk_pixbuf_get_from_drawable(NULL,
                                                   GDK_DRAWABLE(pixmap),
                                                   colormap,
                                                   0, 0, 0, 0, w, h);

  // Finally, we convert our pixbuf into a type we can use.
  SkBitmap border = GdkPixbufToImageSkia(pixbuf);
  g_object_unref(pixbuf);
  g_object_unref(pixmap);
  gtk_widget_destroy(window);

  return border;
}

void Gtk2UI::ClearAllThemeData() {
  STLDeleteValues(&gtk_images_);
}

}  // namespace libgtk2ui

ui::LinuxUI* BuildGtk2UI() {
  return new libgtk2ui::Gtk2UI;
}
