// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "widevine_cdm_version.h"  //  In SHARED_INTERMEDIATE_DIR.


void MeasureTimeToLoadNativeLibrary(const base::FilePath& library_name) {
  base::FilePath output_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_MODULE, &output_dir));
  base::FilePath library_path = output_dir.Append(library_name);
  std::string error;
  base::TimeTicks start = base::TimeTicks::HighResNow();
  base::NativeLibrary native_library =
      base::LoadNativeLibrary(library_path, &error);
  double delta = (base::TimeTicks::HighResNow() - start).InMillisecondsF();
  ASSERT_TRUE(native_library) << "Error loading library:\n" << error;
  base::UnloadNativeLibrary(native_library);
  perf_test::PrintResult("time_to_load_library",
                         "",
                         library_name.AsUTF8Unsafe(),
                         delta,
                         "ms",
                         true);
}

// Use the base name of the library to dynamically get the platform specific
// name. See base::GetNativeLibraryName() for details.
void MeasureTimeToLoadNativeLibraryByBaseName(
    const std::string& base_library_name) {
  MeasureTimeToLoadNativeLibrary(base::FilePath::FromUTF16Unsafe(
      base::GetNativeLibraryName(base::ASCIIToUTF16(base_library_name))));
}

#if defined(ENABLE_PEPPER_CDMS)
#if defined(WIDEVINE_CDM_AVAILABLE)
TEST(LoadCDMPerfTest, Widevine) {
  MeasureTimeToLoadNativeLibrary(
      base::FilePath(FILE_PATH_LITERAL(kWidevineCdmFileName)));
}

TEST(LoadCDMPerfTest, WidevineAdapter) {
  MeasureTimeToLoadNativeLibrary(
      base::FilePath(FILE_PATH_LITERAL(kWidevineCdmAdapterFileName)));
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

TEST(LoadCDMPerfTest, ExternalClearKey) {
  MeasureTimeToLoadNativeLibraryByBaseName("clearkeycdm");
}

TEST(LoadCDMPerfTest, ExternalClearKeyAdapter) {
  MeasureTimeToLoadNativeLibraryByBaseName("clearkeycdmadapter");
}
#endif  // defined(ENABLE_PEPPER_CDMS)
