// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DB_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DB_FACTORY_H_
#pragma once

#include "base/memory/scoped_ptr.h"

class FilePath;

namespace gdata {

class GDataDB;

namespace db_factory {

// Factory method to create an instance of GDataDB.
scoped_ptr<GDataDB> CreateGDataDB(const FilePath& db_path);

}  // namespace db_factory
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DB_FACTORY_H_
