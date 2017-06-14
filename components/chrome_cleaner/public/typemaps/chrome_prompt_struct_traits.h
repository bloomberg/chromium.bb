// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROME_CLEANER_PUBLIC_TYPEMAPS_CHROME_CLEANER_STRUCT_TRAITS_H_
#define COMPONENTS_CHROME_CLEANER_PUBLIC_TYPEMAPS_CHROME_CLEANER_STRUCT_TRAITS_H_

#include "base/files/file_path.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"

namespace mojo {

template <>
struct StructTraits<chrome_cleaner::mojom::FilePathDataView, base::FilePath> {
  static ConstCArray<uint16_t> value(const base::FilePath& file_path);
  static bool Read(chrome_cleaner::mojom::FilePathDataView path_view,
                   base::FilePath* out);
};

}  // namespace mojo

#endif  // COMPONENTS_CHROME_CLEANER_PUBLIC_TYPEMAPS_CHROME_CLEANER_STRUCT_TRAITS_H_
