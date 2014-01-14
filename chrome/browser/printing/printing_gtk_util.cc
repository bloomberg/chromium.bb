// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printing_gtk_util.h"

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "printing/print_settings.h"
#include "printing/printing_context_linux.h"
#include "printing/units.h"

gfx::Size GetPdfPaperSizeDeviceUnitsGtk(
    printing::PrintingContextLinux* context) {
  GtkPageSetup* page_setup = gtk_page_setup_new();

  gfx::SizeF paper_size(
      gtk_page_setup_get_paper_width(page_setup, GTK_UNIT_INCH),
      gtk_page_setup_get_paper_height(page_setup, GTK_UNIT_INCH));

  g_object_unref(page_setup);

  const printing::PrintSettings& settings = context->settings();

  return gfx::Size(
      paper_size.width() * settings.device_units_per_inch(),
      paper_size.height() * settings.device_units_per_inch());
}
