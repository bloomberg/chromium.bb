// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CLIPBOARD_STRUCT_TRAITS_H_
#define CONTENT_COMMON_CLIPBOARD_STRUCT_TRAITS_H_

#include "content/common/clipboard.mojom.h"
#include "content/common/clipboard_format.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<content::mojom::ClipboardFormat, content::ClipboardFormat> {
  static content::mojom::ClipboardFormat ToMojom(
      content::ClipboardFormat clipboard_format) {
    switch (clipboard_format) {
      case content::CLIPBOARD_FORMAT_PLAINTEXT:
        return content::mojom::ClipboardFormat::kPlaintext;
      case content::CLIPBOARD_FORMAT_HTML:
        return content::mojom::ClipboardFormat::kHtml;
      case content::CLIPBOARD_FORMAT_SMART_PASTE:
        return content::mojom::ClipboardFormat::kSmartPaste;
      case content::CLIPBOARD_FORMAT_BOOKMARK:
        return content::mojom::ClipboardFormat::kBookmark;
    }
    NOTREACHED();
    return content::mojom::ClipboardFormat::kPlaintext;
  }

  static bool FromMojom(content::mojom::ClipboardFormat clipboard_format,
                        content::ClipboardFormat* out) {
    switch (clipboard_format) {
      case content::mojom::ClipboardFormat::kPlaintext:
        *out = content::CLIPBOARD_FORMAT_PLAINTEXT;
        return true;
      case content::mojom::ClipboardFormat::kHtml:
        *out = content::CLIPBOARD_FORMAT_HTML;
        return true;
      case content::mojom::ClipboardFormat::kSmartPaste:
        *out = content::CLIPBOARD_FORMAT_SMART_PASTE;
        return true;
      case content::mojom::ClipboardFormat::kBookmark:
        *out = content::CLIPBOARD_FORMAT_BOOKMARK;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_CLIPBOARD_STRUCT_TRAITS_H_
