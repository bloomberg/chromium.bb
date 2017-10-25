// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_manager_utils.h"

#include "base/command_line.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/common/print_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "printing/print_settings.h"

namespace printing {

// A temporary flag which makes supporting both paths for OOPIF and non-OOPIF
// printing easier.
static bool g_oopif_enabled = false;

void SetOopifEnabled() {
  g_oopif_enabled = true;
}

bool IsOopifEnabled() {
  return g_oopif_enabled;
}

void CreateCompositeClientIfNeeded(content::WebContents* web_contents,
                                   bool for_preview) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess) ||
      base::FeatureList::IsEnabled(features::kTopDocumentIsolation)) {
    // For cases need to support OOPIFs.
    PrintCompositeClient::CreateForWebContents(web_contents);
    if (for_preview) {
      PrintCompositeClient::FromWebContents(web_contents)
          ->set_for_preview(true);
    }
    SetOopifEnabled();
  }
}

void RenderParamsFromPrintSettings(const PrintSettings& settings,
                                   PrintMsg_Print_Params* params) {
  params->page_size = settings.page_setup_device_units().physical_size();
  params->content_size.SetSize(
      settings.page_setup_device_units().content_area().width(),
      settings.page_setup_device_units().content_area().height());
  params->printable_area.SetRect(
      settings.page_setup_device_units().printable_area().x(),
      settings.page_setup_device_units().printable_area().y(),
      settings.page_setup_device_units().printable_area().width(),
      settings.page_setup_device_units().printable_area().height());
  params->margin_top = settings.page_setup_device_units().content_area().y();
  params->margin_left = settings.page_setup_device_units().content_area().x();
  params->dpi = settings.dpi();
  params->scale_factor = settings.scale_factor();
  params->rasterize_pdf = settings.rasterize_pdf();
  // Always use an invalid cookie.
  params->document_cookie = 0;
  params->selection_only = settings.selection_only();
  params->supports_alpha_blend = settings.supports_alpha_blend();
  params->should_print_backgrounds = settings.should_print_backgrounds();
  params->display_header_footer = settings.display_header_footer();
  params->title = settings.title();
  params->url = settings.url();
  params->printed_doc_type =
      IsOopifEnabled() ? SkiaDocumentType::MSKP : SkiaDocumentType::PDF;
}

}  // namespace printing
