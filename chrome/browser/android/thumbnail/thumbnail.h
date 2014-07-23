// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THUMBNAIL_THUMBNAIL_H_
#define CHROME_BROWSER_ANDROID_THUMBNAIL_THUMBNAIL_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "content/public/browser/android/ui_resource_client_android.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class Time;
}

namespace content {
class UIResourceProvider;
}

namespace gfx {
class Size;
}

typedef int TabId;

class Thumbnail;

class ThumbnailDelegate {
 public:
  virtual void InvalidateCachedThumbnail(Thumbnail* thumbnail) = 0;
  virtual ~ThumbnailDelegate() {}
};

class Thumbnail : public content::UIResourceClientAndroid {
 public:
  static scoped_ptr<Thumbnail> Create(
      TabId tab_id,
      const base::Time& time_stamp,
      float scale,
      content::UIResourceProvider* ui_resource_provider,
      ThumbnailDelegate* thumbnail_delegate);
  virtual ~Thumbnail();

  TabId tab_id() const { return tab_id_; }
  base::Time time_stamp() const { return time_stamp_; }
  float scale() const { return scale_; }
  cc::UIResourceId ui_resource_id() const { return ui_resource_id_; }
  const gfx::SizeF& scaled_content_size() const { return scaled_content_size_; }
  const gfx::SizeF& scaled_data_size() const { return scaled_data_size_; }

  void SetBitmap(const SkBitmap& bitmap);
  void SetCompressedBitmap(skia::RefPtr<SkPixelRef> compressed_bitmap,
                           const gfx::Size& content_size);
  void CreateUIResource();

  // content::UIResourceClient implementation.
  virtual cc::UIResourceBitmap GetBitmap(cc::UIResourceId uid,
                                         bool resource_lost) OVERRIDE;

  // content::UIResourceClientAndroid implementation.
  virtual void UIResourceIsInvalid() OVERRIDE;

 protected:
  Thumbnail(TabId tab_id,
            const base::Time& time_stamp,
            float scale,
            content::UIResourceProvider* ui_resource_provider,
            ThumbnailDelegate* thumbnail_delegate);

  void ClearUIResourceId();

  TabId tab_id_;
  base::Time time_stamp_;
  float scale_;

  gfx::SizeF scaled_content_size_;
  gfx::SizeF scaled_data_size_;

  cc::UIResourceBitmap bitmap_;
  cc::UIResourceId ui_resource_id_;

  bool retrieved_;

  content::UIResourceProvider* ui_resource_provider_;
  ThumbnailDelegate* thumbnail_delegate_;

  DISALLOW_COPY_AND_ASSIGN(Thumbnail);
};

#endif  // CHROME_BROWSER_ANDROID_THUMBNAIL_THUMBNAIL_H_
