// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_print_settings_initializer.h"

#include "content/browser/browser_thread_impl.h"
#include "printing/printing_context.h"
#include "printing/units.h"

namespace content {

namespace {

// |kAccessorThread| is the thread which will call |GetDefaultPrintSettings|.
const BrowserThread::ID kAccessorThread = BrowserThread::IO;

// Print units conversion functions.
int32_t DeviceUnitsInPoints(int32_t device_units,
                            int32_t device_units_per_inch) {
#if defined(ENABLE_PRINTING)
  return printing::ConvertUnit(device_units, printing::kPointsPerInch,
                               device_units_per_inch);
#else
  return 0;
#endif
}

PP_Size PrintSizeToPPPrintSize(const gfx::Size& print_size,
                               int32_t device_units_per_inch) {
  PP_Size result;
  result.width = DeviceUnitsInPoints(print_size.width(), device_units_per_inch);
  result.height = DeviceUnitsInPoints(print_size.height(),
                                      device_units_per_inch);
  return result;

}

PP_Rect PrintAreaToPPPrintArea(const gfx::Rect& print_area,
                               int32_t device_units_per_inch) {
  PP_Rect result;
  result.point.x = DeviceUnitsInPoints(print_area.origin().x(),
                                       device_units_per_inch);
  result.point.y = DeviceUnitsInPoints(print_area.origin().y(),
                                       device_units_per_inch);
  result.size = PrintSizeToPPPrintSize(print_area.size(),
                                       device_units_per_inch);
  return result;
}

}  // namespace

PepperPrintSettingsInitializer::PepperPrintSettingsInitializer()
    : initialized_(false) {
}

void PepperPrintSettingsInitializer::Initialize() {
  // Multiple initializations are OK, it would just refresh the settings.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&PepperPrintSettingsInitializer::DoInitialize, this));
}

bool PepperPrintSettingsInitializer::GetDefaultPrintSettings(
    PP_PrintSettings_Dev* settings) const {
  DCHECK(BrowserThread::CurrentlyOn(kAccessorThread));
  if (!initialized_)
    return false;
  *settings = default_print_settings_;
  return true;
}

PepperPrintSettingsInitializer::~PepperPrintSettingsInitializer() {
}

void PepperPrintSettingsInitializer::DoInitialize() {
// If printing isn't enabled, we don't do any initialization.
#if defined(ENABLE_PRINTING)
  // This function should run on the UI thread because |PrintingContext| methods
  // call into platform APIs.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PP_PrintSettings_Dev default_settings;
  scoped_ptr<printing::PrintingContext> context(
      printing::PrintingContext::Create(""));
  if (!context.get())
    return;
  context->UseDefaultSettings();
  const printing::PrintSettings& print_settings = context->settings();
  const printing::PageSetup& page_setup =
    print_settings.page_setup_device_units();
  int device_units_per_inch = print_settings.device_units_per_inch();
  default_settings.printable_area =
    PrintAreaToPPPrintArea(page_setup.printable_area(), device_units_per_inch);
  default_settings.content_area =
    PrintAreaToPPPrintArea(page_setup.content_area(), device_units_per_inch);
  default_settings.paper_size =
    PrintSizeToPPPrintSize(page_setup.physical_size(), device_units_per_inch);
  default_settings.dpi = print_settings.dpi();

  // The remainder of the attributes are hard-coded to the defaults as set
  // elsewhere.
  default_settings.orientation = PP_PRINTORIENTATION_NORMAL;
  default_settings.grayscale = PP_FALSE;
  default_settings.print_scaling_option = PP_PRINTSCALINGOPTION_SOURCE_SIZE;

  // TODO(raymes): Should be computed in the same way as
  // |PluginInstance::GetPreferredPrintOutputFormat|.
  // |PP_PRINTOUTPUTFORMAT_PDF| is currently the only supported format though,
  // so just make it the default.
  default_settings.format = PP_PRINTOUTPUTFORMAT_PDF;

  BrowserThread::PostTask(kAccessorThread, FROM_HERE,
    base::Bind(&PepperPrintSettingsInitializer::SetDefaultPrintSettings,
               this, default_settings));
#endif
}

void PepperPrintSettingsInitializer::SetDefaultPrintSettings(
    const PP_PrintSettings_Dev& settings) {
  DCHECK(BrowserThread::CurrentlyOn(kAccessorThread));
  initialized_ = true;
  default_print_settings_ = settings;
}

}  // namespace content
