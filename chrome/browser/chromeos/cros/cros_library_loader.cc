// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library_loader.h"

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/metrics/histogram.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/cros/chromeos_cros_api.h"

namespace chromeos {

namespace {

void addLibcrosTimeHistogram(const char* name, const base::TimeDelta& delta) {
  static const base::TimeDelta min_time = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta max_time = base::TimeDelta::FromSeconds(1);
  const size_t bucket_count(10);
  DCHECK(name);
  scoped_refptr<base::Histogram> counter = base::Histogram::FactoryTimeGet(
      std::string(name),
      min_time,
      max_time,
      bucket_count,
      base::Histogram::kNoFlags);
  if (counter.get()) {
    counter->AddTime(delta);
    VLOG(1) << "Cros Time: " << name << ": " << delta.InMilliseconds() << "ms.";
  }
}

}  // namespace

bool CrosLibraryLoader::Load(std::string* load_error_string) {
  bool loaded = false;
  FilePath path;
  if (PathService::Get(chrome::FILE_CHROMEOS_API, &path)) {
    loaded = LoadLibcros(path.value().c_str(), *load_error_string);
    if (loaded)
      SetLibcrosTimeHistogramFunction(addLibcrosTimeHistogram);
  }

  if (!loaded) {
    LOG(ERROR) << "Problem loading chromeos shared object: "
               << *load_error_string;
  }
  return loaded;
}

}   // namespace chromeos
