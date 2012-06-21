// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/library_loader.h"

#include <dlfcn.h>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/cros/chromeos_cros_api.h"

namespace chromeos {

namespace {

void AddLibcrosTimeHistogram(const char* name, const base::TimeDelta& delta) {
  static const base::TimeDelta min_time = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta max_time = base::TimeDelta::FromSeconds(1);
  const size_t bucket_count(10);
  DCHECK(name);
  base::Histogram* counter = base::Histogram::FactoryTimeGet(
      std::string(name),
      min_time,
      max_time,
      bucket_count,
      base::Histogram::kNoFlags);
  counter->AddTime(delta);
  VLOG(1) << "Cros Time: " << name << ": " << delta.InMilliseconds() << "ms.";
}

}  // namespace

class LibraryLoaderImpl : public LibraryLoader {
 public:
  LibraryLoaderImpl();

  // LibraryLoader:
  virtual bool Load(std::string* load_error_string) OVERRIDE;
};

LibraryLoaderImpl::LibraryLoaderImpl() {}

bool LibraryLoaderImpl::Load(std::string* load_error_string) {
  bool loaded = false;
  FilePath path;
  if (PathService::Get(chrome::FILE_CHROMEOS_API, &path)) {
    loaded = LoadLibcros(path.value().c_str(), *load_error_string);
    if (loaded)
      SetLibcrosTimeHistogramFunction(AddLibcrosTimeHistogram);
  }

  if (!loaded) {
    LOG(ERROR) << "Problem loading chromeos shared object: "
               << *load_error_string;
  }
  return loaded;
}

// static
LibraryLoader* LibraryLoader::GetImpl() {
  return new LibraryLoaderImpl();
}

}   // namespace chromeos
