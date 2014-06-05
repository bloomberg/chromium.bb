// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_CDD_CONVERSION_H_
#define CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_CDD_CONVERSION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace printing {
struct PrinterSemanticCapsAndDefaults;
}

namespace cloud_print {

scoped_ptr<base::DictionaryValue> PrinterSemanticCapsAndDefaultsToCdd(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info);

}  // namespace cloud_print

#endif  // CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_CDD_CONVERSION_H_
