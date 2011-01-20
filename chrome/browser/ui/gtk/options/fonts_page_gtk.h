// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The fonts page of the fonts & languages options dialog, which contains font
// family and size settings, as well as the default encoding option.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_FONTS_PAGE_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_FONTS_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/options/options_page_base.h"
#include "ui/base/gtk/gtk_signal.h"

class DefaultEncodingComboboxModel;

class FontsPageGtk : public OptionsPageBase {
 public:
  explicit FontsPageGtk(Profile* profile);
  virtual ~FontsPageGtk();

  GtkWidget* get_page_widget() const { return page_; }

 private:
  void Init();
  void InitDefaultEncodingComboBox();

  // Overridden from OptionsPageBase.
  virtual void NotifyPrefChanged(const std::string* pref_name);

  // Retrieve the font selection from the button and save it to the prefs.  Also
  // ensure the button(s) are displayed in the proper size, as the
  // GtkFontSelector returns the value in points not pixels.
  void SetFontsFromButton(StringPrefMember* name_pref,
                          IntegerPrefMember* size_pref,
                          GtkWidget* font_button);

  CHROMEGTK_CALLBACK_0(FontsPageGtk, void, OnSerifFontSet);
  CHROMEGTK_CALLBACK_0(FontsPageGtk, void, OnSansFontSet);
  CHROMEGTK_CALLBACK_0(FontsPageGtk, void, OnFixedFontSet);
  CHROMEGTK_CALLBACK_0(FontsPageGtk, void, OnDefaultEncodingChanged);

  // The font chooser widgets
  GtkWidget* serif_font_button_;
  GtkWidget* sans_font_button_;
  GtkWidget* fixed_font_button_;

  // The default encoding combobox widget.
  GtkWidget* default_encoding_combobox_;
  scoped_ptr<DefaultEncodingComboboxModel> default_encoding_combobox_model_;

  // The widget containing the options for this page.
  GtkWidget* page_;

  // Font name preferences.
  StringPrefMember serif_name_;
  StringPrefMember sans_serif_name_;
  StringPrefMember fixed_width_name_;

  // Font size preferences, in pixels.
  IntegerPrefMember variable_width_size_;
  IntegerPrefMember fixed_width_size_;

  // Default encoding preference.
  StringPrefMember default_encoding_;

  DISALLOW_COPY_AND_ASSIGN(FontsPageGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_FONTS_PAGE_GTK_H_
