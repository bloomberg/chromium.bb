// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_

#include <pango/pango.h>
#include <vector>

#include "chrome/browser/autofill/autofill_popup_view.h"
#include "content/public/browser/keyboard_listener.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/base/gtk/gtk_signal.h"

class GtkThemeService;
class Profile;

namespace content {
class RenderViewHost;
}

namespace gfx {
class Rect;
}

typedef struct _GdkEventButton GdkEventButton;
typedef struct _GdkEventCrossing GdkEventCrossing;
typedef struct _GdkEventExpose GdkEventExpose;
typedef struct _GdkEventKey GdkEventKey;
typedef struct _GdkEventMotion GdkEventMotion;
typedef struct _GdkColor GdkColor;
typedef struct _GtkWidget GtkWidget;

class AutofillPopupViewGtk : public AutofillPopupView,
                             public KeyboardListener {
 public:
  AutofillPopupViewGtk(content::WebContents* web_contents,
                       GtkThemeService* theme_service,
                       AutofillExternalDelegate* external_delegate,
                       const gfx::Rect& element_bounds,
                       GtkWidget* parent);
  virtual ~AutofillPopupViewGtk();

  // AutofillPopupView implementation.
  virtual void Hide() OVERRIDE;

 protected:
  // AutofillPopupView implementations.
  virtual void ShowInternal() OVERRIDE;
  virtual void InvalidateRow(size_t row) OVERRIDE;
  virtual void UpdateBoundsAndRedrawPopupInternal() OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_1(AutofillPopupViewGtk, gboolean, HandleButtonRelease,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(AutofillPopupViewGtk, gboolean, HandleExpose,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(AutofillPopupViewGtk, gboolean, HandleLeave,
                       GdkEventCrossing*)
  CHROMEGTK_CALLBACK_1(AutofillPopupViewGtk, gboolean, HandleMotion,
                       GdkEventMotion*);

  //  KeyboardListener implementation.
  virtual bool HandleKeyPressEvent(GdkEventKey* event) OVERRIDE;

  // Set up the pango layout to display the autofill results.
  void SetupLayout(const gfx::Rect& window_rect);

  // Set up the pango layout to print the given text and have it's width match
  // the text's (this gives us better control when placing the text box).
  void SetLayoutText(const string16& text,
                     const gfx::Font& font,
                     const GdkColor text_color);

  // Draw the separator as the given |separator_rect|.
  void DrawSeparator(cairo_t* cairo_context, const gfx::Rect& separator_rect);

  // Draw the given autofill entry in |entry_rect|.
  void DrawAutofillEntry(cairo_t* cairo_context,
                         size_t index,
                         const gfx::Rect& entry_rect);

  // Set the initial bounds of the popup to show, including the placement
  // of it.
  void SetInitialBounds();

  GtkWidget* parent_;  // Weak reference.
  GtkWidget* window_;  // Strong reference.
  PangoLayout* layout_;  // Strong reference
  GtkThemeService* theme_service_;

  content::RenderViewHost* render_view_host_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_AUTOFILL_AUTOFILL_POPUP_VIEW_GTK_H_
