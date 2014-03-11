// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cdd_conversion_win.h"

#include "components/cloud_devices/printer_description.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/win_helper.h"

namespace cloud_print {

bool IsValidCjt(const std::string& print_ticket_data) {
  cloud_devices::CloudDeviceDescription description;
  return description.InitFromString(print_ticket_data);
}

scoped_ptr<DEVMODE, base::FreeDeleter> CjtToDevMode(
    const base::string16& printer_name,
    const std::string& print_ticket) {
  scoped_ptr<DEVMODE, base::FreeDeleter> dev_mode;

  cloud_devices::CloudDeviceDescription description;
  if (!description.InitFromString(print_ticket))
    return dev_mode.Pass();

  using namespace cloud_devices::printer;
  printing::ScopedPrinterHandle printer;
  if (!printer.OpenPrinter(printer_name.c_str()))
    return dev_mode.Pass();

  {
    ColorTicketItem color;
    if (color.LoadFrom(description)) {
      bool is_color = color.value().type == STANDARD_COLOR;
      dev_mode = CreateDevModeWithColor(printer, printer_name, is_color);
    } else {
      dev_mode = printing::CreateDevMode(printer, NULL);
    }
  }

  if (!dev_mode)
    return dev_mode.Pass();

  ColorTicketItem color;
  DuplexTicketItem duplex;
  OrientationTicketItem orientation;
  MarginsTicketItem margins;
  DpiTicketItem dpi;
  FitToPageTicketItem fit_to_page;
  MediaTicketItem media;
  CopiesTicketItem copies;
  PageRangeTicketItem page_range;
  CollateTicketItem collate;
  ReverseTicketItem reverse;

  if (orientation.LoadFrom(description)) {
    dev_mode->dmFields |= DM_ORIENTATION;
    if (orientation.value() == LANDSCAPE) {
      dev_mode->dmOrientation = DMORIENT_LANDSCAPE;
    } else {
      dev_mode->dmOrientation = DMORIENT_PORTRAIT;
    }
  }

  if (color.LoadFrom(description)) {
    dev_mode->dmFields |= DM_COLOR;
    if (color.value().type == STANDARD_MONOCHROME) {
      dev_mode->dmColor = DMCOLOR_MONOCHROME;
    } else if (color.value().type == STANDARD_COLOR) {
      dev_mode->dmColor = DMCOLOR_COLOR;
    } else {
      NOTREACHED();
    }
  }

  if (duplex.LoadFrom(description)) {
    dev_mode->dmFields |= DM_DUPLEX;
    if (duplex.value() == NO_DUPLEX) {
      dev_mode->dmDuplex = DMDUP_SIMPLEX;
    } else if (duplex.value() == LONG_EDGE) {
      dev_mode->dmDuplex = DMDUP_VERTICAL;
    } else if (duplex.value() == SHORT_EDGE) {
      dev_mode->dmDuplex = DMDUP_HORIZONTAL;
    } else {
      NOTREACHED();
    }
  }

  if (copies.LoadFrom(description)) {
    dev_mode->dmFields |= DM_COPIES;
    dev_mode->dmCopies = copies.value();
  }

  if (dpi.LoadFrom(description)) {
    if (dpi.value().horizontal > 0) {
      dev_mode->dmFields |= DM_PRINTQUALITY;
      dev_mode->dmPrintQuality = dpi.value().horizontal;
    }
    if (dpi.value().vertical > 0) {
      dev_mode->dmFields |= DM_YRESOLUTION;
      dev_mode->dmYResolution = dpi.value().vertical;
    }
  }

  if (collate.LoadFrom(description)) {
    dev_mode->dmFields |= DM_COLLATE;
    dev_mode->dmCollate = (collate.value() ? DMCOLLATE_TRUE : DMCOLLATE_FALSE);
  }

  if (media.LoadFrom(description)) {
    static const size_t kFromUm = 100;  // Windows uses 0.1mm.
    int width = media.value().width_um / kFromUm;
    int height = media.value().height_um / kFromUm;
    if (width > 0) {
      dev_mode->dmFields |= DM_PAPERWIDTH;
      dev_mode->dmPaperWidth = width;
    }
    if (height > 0) {
      dev_mode->dmFields |= DM_PAPERLENGTH;
      dev_mode->dmPaperLength = height;
    }
  }

  return printing::CreateDevMode(printer, dev_mode.get());
}

std::string CapabilitiesToCdd(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  using namespace cloud_devices::printer;
  cloud_devices::CloudDeviceDescription description;

  ContentTypesCapability content_types;
  content_types.AddOption("application/pdf");
  content_types.SaveTo(&description);

  ColorCapability color;
  if (semantic_info.color_default || semantic_info.color_changeable) {
    color.AddDefaultOption(Color(STANDARD_COLOR), semantic_info.color_default);
  }

  if (!semantic_info.color_default || semantic_info.color_changeable) {
    color.AddDefaultOption(Color(STANDARD_MONOCHROME),
                           !semantic_info.color_default);
  }
  color.SaveTo(&description);

  if (semantic_info.duplex_capable) {
    DuplexCapability duplex;
    duplex.AddDefaultOption(
        NO_DUPLEX, semantic_info.duplex_default == printing::SIMPLEX);
    duplex.AddDefaultOption(
        LONG_EDGE, semantic_info.duplex_default == printing::LONG_EDGE);
    duplex.AddDefaultOption(
        SHORT_EDGE, semantic_info.duplex_default == printing::SHORT_EDGE);
    duplex.SaveTo(&description);
  }

  if (!semantic_info.papers.empty()) {
    Media default_media(semantic_info.default_paper.name,
                        semantic_info.default_paper.size_um.width(),
                        semantic_info.default_paper.size_um.height());
    default_media.MatchBySize();

    MediaCapability media;
    bool is_default_set = false;
    for (size_t i = 0; i < semantic_info.papers.size(); ++i) {
      gfx::Size paper_size = semantic_info.papers[i].size_um;
      if (paper_size.width() > paper_size.height())
        paper_size.SetSize(paper_size.height(), paper_size.width());
      Media new_media(semantic_info.papers[i].name, paper_size.width(),
                      paper_size.height());
      new_media.MatchBySize();
      if (new_media.IsValid() && !media.Contains(new_media)) {
        if (!default_media.IsValid())
          default_media = new_media;
        media.AddDefaultOption(new_media, new_media == default_media);
        is_default_set = is_default_set || (new_media == default_media);
      }
    }
    if (!is_default_set && default_media.IsValid())
      media.AddDefaultOption(default_media, true);

    if (media.IsValid()) {
      media.SaveTo(&description);
    } else {
      NOTREACHED();
    }
  }

  if (semantic_info.collate_capable) {
    CollateCapability collate;
    collate.set_default_value(semantic_info.collate_default);
    collate.SaveTo(&description);
  }

  if (semantic_info.copies_capable) {
    CopiesCapability copies;
    copies.SaveTo(&description);
  }

  if (!semantic_info.dpis.empty()) {
    DpiCapability dpi;
    Dpi default_dpi(semantic_info.default_dpi.width(),
                    semantic_info.default_dpi.height());
    bool is_default_set = false;
    for (size_t i = 0; i < semantic_info.dpis.size(); ++i) {
      Dpi new_dpi(semantic_info.dpis[i].width(),
                  semantic_info.dpis[i].height());
      if (new_dpi.IsValid() && !dpi.Contains(new_dpi)) {
        if (!default_dpi.IsValid())
          default_dpi = new_dpi;
        dpi.AddDefaultOption(new_dpi, new_dpi == default_dpi);
        is_default_set = is_default_set || (new_dpi == default_dpi);
      }
    }
    if (!is_default_set && default_dpi.IsValid())
      dpi.AddDefaultOption(default_dpi, true);
    if (dpi.IsValid()) {
      dpi.SaveTo(&description);
    } else {
      NOTREACHED();
    }
  }

  OrientationCapability orientation;
  orientation.AddDefaultOption(PORTRAIT, true);
  orientation.AddOption(LANDSCAPE);
  orientation.AddOption(AUTO_ORIENTATION);
  orientation.SaveTo(&description);

  return description.ToString();
}

}  // namespace cloud_print
