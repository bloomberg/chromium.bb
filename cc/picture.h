// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_H_
#define CC_PICTURE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/rect.h"

namespace cc {

class ContentLayerClient;

class CC_EXPORT Picture
    : public base::RefCountedThreadSafe<Picture> {
public:
  scoped_refptr<Picture> create(ContentLayerClient*, gfx::Rect);

  const gfx::Rect& rect();
  scoped_refptr<Picture> clone();

protected:
  Picture();
  ~Picture();

  gfx::Rect m_rect;
  SkPicture m_picture;

private:
  friend class base::RefCountedThreadSafe<Picture>;

  DISALLOW_COPY_AND_ASSIGN(Picture);
};

}  // namespace cc

#endif  // CC_PICTURE_H_
