// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "widevine_cdm_version.h"  //  In SHARED_INTERMEDIATE_DIR.

// Measures the size (bytes) and time to load (sec) of a native library.
void MeasureSizeAndTimeToLoadNativeLibrary(const base::FilePath& library_name) {
  base::FilePath output_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_MODULE, &output_dir));
  base::FilePath library_path = output_dir.Append(library_name);
  ASSERT_TRUE(base::PathExists(library_path)) << library_path.value();

  int64 size = 0;
  ASSERT_TRUE(base::GetFileSize(library_path, &size));
  perf_test::PrintResult("library_size",
                         "",
                         library_name.AsUTF8Unsafe(),
                         static_cast<size_t>(size),
                         "bytes",
                         true);

  base::NativeLibraryLoadError error;
  base::TimeTicks start = base::TimeTicks::HighResNow();
  base::NativeLibrary native_library =
      base::LoadNativeLibrary(library_path, &error);
  double delta = (base::TimeTicks::HighResNow() - start).InMillisecondsF();
  ASSERT_TRUE(native_library) << "Error loading library: " << error.ToString();
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
void MeasureSizeAndTimeToLoadNativeLibraryByBaseName(
    const std::string& base_library_name) {
  MeasureSizeAndTimeToLoadNativeLibrary(base::FilePath::FromUTF16Unsafe(
      base::GetNativeLibraryName(base::ASCIIToUTF16(base_library_name))));
}

#if defined(ENABLE_PEPPER_CDMS)
#if defined(WIDEVINE_CDM_AVAILABLE)
TEST(LoadCDMPerfTest, Widevine) {
  MeasureSizeAndTimeToLoadNativeLibrary(
      base::FilePath::FromUTF8Unsafe(kWidevineCdmFileName));
}

TEST(LoadCDMPerfTest, WidevineAdapter) {
  MeasureSizeAndTimeToLoadNativeLibrary(
      base::FilePath::FromUTF8Unsafe(kWidevineCdmAdapterFileName));
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

TEST(LoadCDMPerfTest, ExternalClearKey) {
#if defined(OS_MACOSX)
  MeasureSizeAndTimeToLoadNativeLibrary(
    base::FilePath::FromUTF8Unsafe("libclearkeycdm.dylib"));
#else
  MeasureSizeAndTimeToLoadNativeLibraryByBaseName("clearkeycdm");
#endif  // defined(OS_MACOSX)
}

TEST(LoadCDMPerfTest, ExternalClearKeyAdapter) {
#if defined(OS_MACOSX)
  MeasureSizeAndTimeToLoadNativeLibrary(
    base::FilePath::FromUTF8Unsafe("clearkeycdmadapter.plugin"));
#else
  MeasureSizeAndTimeToLoadNativeLibraryByBaseName("clearkeycdmadapter");
#endif  // defined(OS_MACOSX)
}
#endif  // defined(ENABLE_PEPPER_CDMS)
