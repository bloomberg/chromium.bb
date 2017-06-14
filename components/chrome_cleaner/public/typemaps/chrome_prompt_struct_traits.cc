// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chrome_cleaner/public/typemaps/chrome_prompt_struct_traits.h"

namespace mojo {

// static
ConstCArray<uint16_t>
StructTraits<chrome_cleaner::mojom::FilePathDataView, base::FilePath>::value(
    const base::FilePath& file_path) {
#if defined(OS_WIN)
  return ConstCArray<uint16_t>(
      file_path.value().size(),
      reinterpret_cast<const uint16_t*>(file_path.value().data()));
#else
  NOTREACHED();
  return ConstCArray<uint16_t>();
#endif
}

// static
bool StructTraits<chrome_cleaner::mojom::FilePathDataView,
                  base::FilePath>::Read(chrome_cleaner::mojom::FilePathDataView
                                            path_view,
                                        base::FilePath* out) {
#if defined(OS_WIN)
  ArrayDataView<uint16_t> view;
  path_view.GetValueDataView(&view);
  base::FilePath path = base::FilePath(base::string16(
      reinterpret_cast<const base::char16*>(view.data()), view.size()));
  *out = std::move(path);
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

}  // namespace mojo
