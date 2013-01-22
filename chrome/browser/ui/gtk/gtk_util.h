// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_UTIL_H_
#define CHROME_BROWSER_UI_GTK_GTK_UTIL_H_

#include <gtk/gtk.h>
#include <string>
#include <vector>

#include "base/string16.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

typedef struct _cairo cairo_t;
typedef struct _GdkColor GdkColor;
typedef struct _GtkWidget GtkWidget;

class BrowserWindow;
class GtkThemeService;
class GURL;
class Profile;

namespace gfx {
class Image;
}

namespace gtk_util {

// Create a table of labeled controls, using proper spacing and alignment.
// Arguments should be pairs of const char*, GtkWidget*, concluding with a
// NULL.  The first argument is a vector in which to place all labels
// produced. It can be NULL if you don't need to keep track of the label
// widgets. The second argument is a color to force the label text to. It can
// be NULL to get the system default.
//
// For example:
// controls = CreateLabeledControlsGroup(NULL,
//                                       "Name:", title_entry_,
//                                       "Folder:", folder_combobox_,
//                                       NULL);
GtkWidget* CreateLabeledControlsGroup(
    std::vector<GtkWidget*>* labels,
    const char* text, ...);

// Create a GtkBin with |child| as its child widget.  This bin will paint a
// border of color |color| with the sizes specified in pixels.
GtkWidget* CreateGtkBorderBin(GtkWidget* child, const GdkColor* color,
                              int top, int bottom, int left, int right);

// Left-align the given GtkMisc and return the same pointer.
GtkWidget* LeftAlignMisc(GtkWidget* misc);

// Create a left-aligned label with the given text in bold.
GtkWidget* CreateBoldLabel(const std::string& text);

// As above, but uses number of characters/lines directly rather than looking up
// a resource.
void GetWidgetSizeFromCharacters(GtkWidget* widget,
                                 double width_chars, double height_lines,
                                 int* width, int* height);

// Calculates the size of given widget based on the size specified in number of
// characters/lines (in locale specific resource file) and font metrics.
// NOTE: Make sure to realize |widget| before using this method, or a default
// font size will be used instead of the actual font size.
void GetWidgetSizeFromResources(GtkWidget* widget,
                                int width_chars, int height_lines,
                                int* width, int* height);

// As above, but a convenience method for configuring dialog size.
// |width_id| and |height_id| are resource IDs for the size.  If either of these
// are set to -1, the respective size will be set to the widget default.
// |resizable| also controls whether the dialog will be resizable
// (this info is also necessary for getting the width-setting code
// right).
void SetWindowSizeFromResources(GtkWindow* window,
                                int width_id, int height_id, bool resizable);

// Puts all browser windows in one window group; this will make any dialog
// spawned app modal.
void MakeAppModalWindowGroup();

// Called after an app modal dialog that used MakeAppModalWindowGroup() was
// dismissed. Returns each browser window to its own window group.
void AppModalDismissedUngroupWindows();

// Remove all children from this container.
void RemoveAllChildren(GtkWidget* container);

// Force the font size of the widget to |size_pixels|.
void ForceFontSizePixels(GtkWidget* widget, double size_pixels);

// Undoes the effects of a previous ForceFontSizePixels() call. Safe to call
// even if ForceFontSizePixels() was never called.
void UndoForceFontSize(GtkWidget* widget);

// Retuns size of the |widget| without window manager decorations.
gfx::Size GetWidgetSize(GtkWidget* widget);

// Converts a point in a widget to screen coordinates.  The point |p| is
// relative to the widget's top-left origin.
void ConvertWidgetPointToScreen(GtkWidget* widget, gfx::Point* p);

// Stick the widget in the given hbox without expanding vertically. The widget
// is packed at the start of the hbox. This is useful for widgets that would
// otherwise expand to fill the vertical space of the hbox
// (e.g. buttons). Returns the vbox that widget was packed in.
GtkWidget* CenterWidgetInHBox(GtkWidget* hbox, GtkWidget* widget,
                              bool pack_at_end, int padding);

// Set that clicking the button with the given mouse buttons will cause a click
// event.
// NOTE: If you need to connect to the button-press-event or
// button-release-event signals, do so before calling this function.
void SetButtonClickableByMouseButtons(GtkWidget* button,
                                      bool left, bool middle, bool right);

// Set that a button causes a page navigation. In particular, it will accept
// middle clicks. Warning: only call this *after* you have connected your
// own handlers for button-press and button-release events, or you will not get
// those events.
void SetButtonTriggersNavigation(GtkWidget* button);

// Returns the mirrored x value for |bounds| if the layout is RTL; otherwise,
// the original value is returned unchanged.
int MirroredLeftPointForRect(GtkWidget* widget, const gfx::Rect& bounds);

// Returns the mirrored right value for |bounds| if the layout is RTL;
// otherwise, the original value is returned unchanged.
int MirroredRightPointForRect(GtkWidget* widget, const gfx::Rect& bounds);

// Returns the mirrored x value for the point |x| if the layout is RTL;
// otherwise, the original value is returned unchanged.
int MirroredXCoordinate(GtkWidget* widget, int x);

// Returns true if the pointer is currently inside the widget.
bool WidgetContainsCursor(GtkWidget* widget);

// Sets the default window icon for all windows created in this app. This icon
// will only be used if a window has not explicitly been assigned an icon
// (e.g. by SetWindowIcon()).
//
// |window| is only used to determine if a themed icon exists. If so, we use
// that icon, otherwise we use the icon packaged with Chrome.
void SetDefaultWindowIcon(GtkWindow* window);

// Sets the icon of |window| to the Chrome product icon, overlaid with
// |profile|'s avatar icon (or the Incognito icon for Incognito windows). It
// first looks for a themed icon, then falls back to the product icons
// packaged with Chrome.
void SetWindowIcon(GtkWindow* window, Profile* profile);

// Sets the icon of |window| to |icon|, overlaid with |profile|'s avatar icon
// (or the Incognito icon for Incognito windows). It first looks for a themed
// icon, then falls back to the product icons packaged with Chrome.
//
// Note that |icon| will be modified by this function.
void SetWindowIcon(GtkWindow* window, Profile* profile, GdkPixbuf* icon);

// Adds an action button with the given text to the dialog. Only useful when you
// want a stock icon but not the stock text to go with it. Returns the button.
GtkWidget* AddButtonToDialog(GtkWidget* dialog, const gchar* text,
                             const gchar* stock_id, gint response_id);

GtkWidget* BuildDialogButton(GtkWidget* dialog, int ids_id,
                             const gchar* stock_id);

GtkWidget* CreateEntryImageHBox(GtkWidget* entry, GtkWidget* image);

// Sets all the foreground color states of |label| to |color|.
void SetLabelColor(GtkWidget* label, const GdkColor* color);

// Adds the given widget to an alignment identing it by |kGroupIndent|.
GtkWidget* IndentWidget(GtkWidget* content);

// Reverses a point in RTL mode. Used in making vectors of GdkPoints for window
// shapes.
GdkPoint MakeBidiGdkPoint(gint x, gint y, gint width, bool ltr);

// Creates a tooltip string to be passed to gtk_widget_set_tooltip_markup from
// the title and URL.
std::string BuildTooltipTitleFor(string16 title, const GURL& url);

// Draws a GTK text entry with the style parameters of GtkEntry
// |offscreen_entry| onto |widget_to_draw_on| in the rectangle |rec|. Drawing
// is only done in the clip rectangle |dirty_rec|.
void DrawTextEntryBackground(GtkWidget* offscreen_entry,
                             GtkWidget* widget_to_draw_on,
                             GdkRectangle* dirty_rec,
                             GdkRectangle* rec);

// Set up the text to be displayed by |layout|.
void SetLayoutText(PangoLayout* layout, const string16& text);

// Draws the background of the toolbar area subject to the expose rectangle
// |event| and starting image tiling from |tabstrip_origin|.
void DrawThemedToolbarBackground(GtkWidget* widget,
                                 cairo_t* cr,
                                 GdkEventExpose* event,
                                 const gfx::Point& tabstrip_origin,
                                 GtkThemeService* provider);

// Draw an entire pixbuf without dithering.
void DrawFullImage(cairo_t* cr,
                   GtkWidget* widget,
                   const gfx::Image& image,
                   gint dest_x,
                   gint dest_y);

// Returns the two colors averaged together.
GdkColor AverageColors(GdkColor color_one, GdkColor color_two);

// Show the image for the given menu item, even if the user's default is to not
// show images. Only to be used for favicons or other menus where the image is
// crucial to its functionality.
void SetAlwaysShowImage(GtkWidget* image_menu_item);

// Get a rectangle corresponding to a widget's allocation relative to its
// toplevel window's origin.
gfx::Rect GetWidgetRectRelativeToToplevel(GtkWidget* widget);

// Don't allow the widget to paint anything, and instead propagate the expose
// to its children. This is similar to calling
//
//   gtk_widget_set_app_paintable(container, TRUE);
//
// except that it will always work, and it should be called after any custom
// expose events are connected.
void SuppressDefaultPainting(GtkWidget* container);

// Safely grabs all input (with X grabs and an application grab), returning true
// for success.
bool GrabAllInput(GtkWidget* widget);

// Returns a rectangle that represents the widget's bounds. The rectangle it
// returns is the same as gtk_widget_get_allocation, but anchored at (0, 0).
gfx::Rect WidgetBounds(GtkWidget* widget);

// Update the timestamp for the given window. This is usually the time of the
// last user event, but on rare occasions we wish to update it despite not
// receiving a user event.
void SetWMLastUserActionTime(GtkWindow* window);

// The current system time, using the format expected by the X server, but not
// retrieved from the X server. NOTE: You should almost never need to use this
// function, instead using the timestamp from the latest GDK event.
guint32 XTimeNow();

// Uses the autocomplete controller for |profile| to convert the contents of the
// PRIMARY selection to a parsed URL. Returns true and sets |url| on success,
// otherwise returns false.
bool URLFromPrimarySelection(Profile* profile, GURL* url);

// Set the colormap of the given window to rgba to allow transparency.
bool AddWindowAlphaChannel(GtkWidget* window);

// Get the default colors for a text entry.  Parameters may be NULL.
void GetTextColors(GdkColor* normal_base,
                   GdkColor* selected_base,
                   GdkColor* normal_text,
                   GdkColor* selected_text);

// Wrappers to show a GtkDialog. On Linux, it merely calls gtk_widget_show_all.
// On ChromeOs, it calls ShowNativeDialog which hosts the its vbox
// in a view based Window.
void ShowDialog(GtkWidget* dialog);
void ShowDialogWithLocalizedSize(GtkWidget* dialog,
                                 int width_id,
                                 int height_id,
                                 bool resizeable);
void ShowDialogWithMinLocalizedWidth(GtkWidget* dialog,
                                     int width_id);

// Wrapper to present a window. On Linux, it just calls gtk_window_present or
// gtk_window_present_with_time for non-zero timestamp.
void PresentWindow(GtkWidget* window, int timestamp);

// Gets dialog window bounds.
gfx::Rect GetDialogBounds(GtkWidget* dialog);

// Returns the stock menu item label for the "preferences" item - returns an
// empty string if no stock item found.
string16 GetStockPreferencesMenuLabel();

// Checks whether a widget is actually visible, i.e. whether it and all its
// ancestors up to its toplevel are visible.
bool IsWidgetAncestryVisible(GtkWidget* widget);

// Sets the given label's size request to |pixel_width|. This will cause the
// label to wrap if it needs to. The reason for this function is that some
// versions of GTK mis-align labels that have a size request and line wrapping,
// and this function hides the complexity of the workaround.
void SetLabelWidth(GtkWidget* label, int pixel_width);

// Make the |label| shrinkable within a GthChromeShrinkableHBox
// It calculates the real size request of a label and set its ellipsize mode to
// PANGO_ELLIPSIZE_END.
// It must be done when the label is mapped (become visible on the screen),
// to make sure the pango can get correct font information for the calculation.
void InitLabelSizeRequestAndEllipsizeMode(GtkWidget* label);

// A helper function for gtk_message_dialog_new() to work around a few KDE 3
// window manager bugs. You should always call it after creating a dialog with
// gtk_message_dialog_new.
void ApplyMessageDialogQuirks(GtkWidget* dialog);

}  // namespace gtk_util

#endif  // CHROME_BROWSER_UI_GTK_GTK_UTIL_H_
