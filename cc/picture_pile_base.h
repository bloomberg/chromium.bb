// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_PILE_BASE_H_
#define CC_PICTURE_PILE_BASE_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/picture.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT PicturePileBase : public base::RefCounted<PicturePileBase> {
 public:
  PicturePileBase();

  void Resize(gfx::Size size);
  gfx::Size size() const { return size_; }
  void SetMinContentsScale(float min_contents_scale);

  void PushPropertiesTo(PicturePileBase* other);

 protected:
  ~PicturePileBase();

  typedef std::list<scoped_refptr<Picture> > Pile;
  Pile pile_;
  gfx::Size size_;
  float min_contents_scale_;
  int buffer_pixels_;

  friend class base::RefCounted<PicturePileBase>;
  DISALLOW_COPY_AND_ASSIGN(PicturePileBase);
};

}  // namespace cc

#endif  // CC_PICTURE_PILE_H_
