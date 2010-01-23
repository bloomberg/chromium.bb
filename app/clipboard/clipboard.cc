// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/clipboard/clipboard.h"

#include "base/gfx/size.h"
#include "base/logging.h"

namespace {

// A compromised renderer could send us bad data, so validate it.
bool IsBitmapSafe(const Clipboard::ObjectMapParams& params) {
  if (params[1].size() != sizeof(gfx::Size))
    return false;
  const gfx::Size* size =
      reinterpret_cast<const gfx::Size*>(&(params[1].front()));
  size_t total_size = size->width();
  // Using INT_MAX not SIZE_T_MAX to put a reasonable bound on things.
  if (INT_MAX / size->width() <= size->height())
    return false;
  total_size *= size->height();
  if (INT_MAX / total_size <= 4)
    return false;
  total_size *= 4;
  return params[0].size() == total_size;
}

}  // namespace

void Clipboard::DispatchObject(ObjectType type, const ObjectMapParams& params) {
  // All types apart from CBF_WEBKIT need at least 1 non-empty param.
  if (type != CBF_WEBKIT && (params.empty() || params[0].empty()))
    return;
  // Some other types need a non-empty 2nd param.
  if ((type == CBF_BOOKMARK || type == CBF_BITMAP || type == CBF_DATA) &&
      (params.size() != 2 || params[1].empty()))
    return;
  switch (type) {
    case CBF_TEXT:
      WriteText(&(params[0].front()), params[0].size());
      break;

    case CBF_HTML:
      if (params.size() == 2) {
        if (params[1].empty())
          return;
        WriteHTML(&(params[0].front()), params[0].size(),
                  &(params[1].front()), params[1].size());
      } else if (params.size() == 1) {
        WriteHTML(&(params[0].front()), params[0].size(), NULL, 0);
      }
      break;

    case CBF_BOOKMARK:
      WriteBookmark(&(params[0].front()), params[0].size(),
                    &(params[1].front()), params[1].size());
      break;

    case CBF_WEBKIT:
      WriteWebSmartPaste();
      break;

    case CBF_BITMAP:
      if (!IsBitmapSafe(params))
        return;
      WriteBitmap(&(params[0].front()), &(params[1].front()));
      break;

#if !defined(OS_MACOSX)
    case CBF_DATA:
      WriteData(&(params[0].front()), params[0].size(),
                &(params[1].front()), params[1].size());
      break;
#endif  // !defined(OS_MACOSX)

    default:
      NOTREACHED();
  }
}
