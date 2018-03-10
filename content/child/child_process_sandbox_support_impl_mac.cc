// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_process_sandbox_support_impl_mac.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "content/common/mac/font_loader.h"
#include "content/public/child/child_thread.h"
#include "mojo/public/cpp/system/buffer.h"

namespace content {

bool LoadFont(CTFontRef font, CGFontRef* out, uint32_t* font_id) {
  uint32_t font_data_size;
  base::ScopedCFTypeRef<CFStringRef> name_ref(CTFontCopyPostScriptName(font));
  base::string16 font_name = SysCFStringRefToUTF16(name_ref);
  float font_point_size = CTFontGetSize(font);
  mojo::ScopedSharedBufferHandle font_data;
  if (!content::ChildThread::Get()->LoadFont(
          font_name, font_point_size, &font_data_size, &font_data, font_id)) {
    *out = nullptr;
    *font_id = 0;
    return false;
  }

  if (font_data_size == 0 || !font_data.is_valid() || *font_id == 0) {
    DLOG(ERROR) << "Bad response from LoadFont() for " << font_name;
    *out = nullptr;
    *font_id = 0;
    return false;
  }

  // TODO(jeremy): Need to call back into the requesting process to make sure
  // that the font isn't already activated, based on the font id.  If it's
  // already activated, don't reactivate it here - https://crbug.com/72727 .
  return FontLoader::CGFontRefFromBuffer(std::move(font_data), font_data_size,
                                         out);
}

}  // namespace content
