// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/win7_location_api_win.h"

#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/win/scoped_propvariant.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/geoposition.h"

namespace content {
namespace {
const double kKnotsToMetresPerSecondConversionFactor = 0.5144;

void ConvertKnotsToMetresPerSecond(double* knots) {
  *knots *= kKnotsToMetresPerSecondConversionFactor;
}

HINSTANCE LoadWin7Library(const string16& lib_name) {
  base::FilePath sys_dir;
  PathService::Get(base::DIR_SYSTEM, &sys_dir);
  return LoadLibrary(sys_dir.Append(lib_name).value().c_str());
}
}

Win7LocationApi::Win7LocationApi()
    : prop_lib_(0),
      PropVariantToDouble_function_(0),
      locator_(0) {
}

void Win7LocationApi::Init(HINSTANCE prop_library,
    PropVariantToDoubleFunction PropVariantToDouble_function,
    ILocation* locator) {
  prop_lib_ = prop_library;
  PropVariantToDouble_function_ = PropVariantToDouble_function;
  locator_ = locator;
}

Win7LocationApi::~Win7LocationApi() {
  if (prop_lib_ != NULL)
    FreeLibrary(prop_lib_);
}

Win7LocationApi* Win7LocationApi::Create() {
  if (!CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kExperimentalLocationFeatures))
    return NULL;

  scoped_ptr<Win7LocationApi> result(new Win7LocationApi);
  // Load probsys.dll
  string16 lib_needed = L"propsys.dll";
  HINSTANCE prop_lib = LoadWin7Library(lib_needed);
  if (!prop_lib)
    return NULL;
  // Get pointer to function.
  PropVariantToDoubleFunction PropVariantToDouble_function;
  PropVariantToDouble_function =
      reinterpret_cast<PropVariantToDoubleFunction>(
          GetProcAddress(prop_lib, "PropVariantToDouble"));
  if (!PropVariantToDouble_function) {
    FreeLibrary(prop_lib);
    return NULL;
  }
  // Create the ILocation object that receives location reports.
  HRESULT result_type;
  CComPtr<ILocation> locator;
  result_type = CoCreateInstance(
      CLSID_Location, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&locator));
  if (!SUCCEEDED(result_type)) {
    FreeLibrary(prop_lib);
    return NULL;
  }
  IID reports_needed[] = { IID_ILatLongReport };
  result_type = locator->RequestPermissions(NULL, reports_needed, 1, TRUE);
  result->Init(prop_lib, PropVariantToDouble_function, locator);
  return result.release();
}

Win7LocationApi* Win7LocationApi::CreateForTesting(
    PropVariantToDoubleFunction PropVariantToDouble_function,
    ILocation* locator) {
  Win7LocationApi* result = new Win7LocationApi;
  result->Init(NULL, PropVariantToDouble_function, locator);
  return result;
}

void Win7LocationApi::GetPosition(Geoposition* position) {
  DCHECK(position);
  position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  if (!locator_)
    return;
  // Try to get a position fix
  if (!GetPositionIfFixed(position))
    return;
  position->error_code = Geoposition::ERROR_CODE_NONE;
  if (!position->Validate()) {
    // GetPositionIfFixed returned true, yet we've not got a valid fix.
    // This shouldn't happen; something went wrong in the conversion.
    NOTREACHED() << "Invalid position from GetPositionIfFixed: lat,long "
                 << position->latitude << "," << position->longitude
                 << " accuracy " << position->accuracy << " time "
                 << position->timestamp.ToDoubleT();
    position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = "Bad fix from Win7 provider";
  }
}

bool Win7LocationApi::GetPositionIfFixed(Geoposition* position) {
  HRESULT result_type;
  CComPtr<ILocationReport> location_report;
  CComPtr<ILatLongReport> lat_long_report;
  result_type = locator_->GetReport(IID_ILatLongReport, &location_report);
  // Checks to see if location access is allowed.
  if (result_type == E_ACCESSDENIED)
    position->error_code = Geoposition::ERROR_CODE_PERMISSION_DENIED;
  // Checks for any other errors while requesting a location report.
  if (!SUCCEEDED(result_type))
    return false;
  result_type = location_report->QueryInterface(&lat_long_report);
  if (!SUCCEEDED(result_type))
    return false;
  result_type = lat_long_report->GetLatitude(&position->latitude);
  if (!SUCCEEDED(result_type))
    return false;
  result_type = lat_long_report->GetLongitude(&position->longitude);
  if (!SUCCEEDED(result_type))
    return false;
  result_type = lat_long_report->GetErrorRadius(&position->accuracy);
  if (!SUCCEEDED(result_type) || position->accuracy <= 0)
    return false;
  double temp_dbl;
  result_type = lat_long_report->GetAltitude(&temp_dbl);
  if (SUCCEEDED(result_type))
    position->altitude = temp_dbl;
  result_type = lat_long_report->GetAltitudeError(&temp_dbl);
  if (SUCCEEDED(result_type))
    position->altitude_accuracy = temp_dbl;
  base::win::ScopedPropVariant propvariant;
  result_type = lat_long_report->GetValue(
      SENSOR_DATA_TYPE_TRUE_HEADING_DEGREES, propvariant.Receive());
  if (SUCCEEDED(result_type))
    PropVariantToDouble_function_(propvariant.get(), &position->heading);
  propvariant.Reset();
  result_type = lat_long_report->GetValue(
      SENSOR_DATA_TYPE_SPEED_KNOTS, propvariant.Receive());
  if (SUCCEEDED(result_type)) {
    PropVariantToDouble_function_(propvariant.get(), &position->speed);
    ConvertKnotsToMetresPerSecond(&position->speed);
  }
  position->timestamp = base::Time::Now();
  return true;
}

bool Win7LocationApi::SetHighAccuracy(bool acc) {
  HRESULT result_type;
  result_type = locator_->SetDesiredAccuracy(IID_ILatLongReport,
      acc ? LOCATION_DESIRED_ACCURACY_HIGH :
            LOCATION_DESIRED_ACCURACY_DEFAULT);
  return SUCCEEDED(result_type);
}

}  // namespace content
