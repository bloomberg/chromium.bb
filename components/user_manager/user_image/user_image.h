// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_
#define COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_

#include <string>
#include <vector>

#include "components/user_manager/user_manager_export.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace user_manager {

// Wrapper class storing a still image and it's raw representation.  Could be
// used for storing profile images and user wallpapers.
class USER_MANAGER_EXPORT UserImage {
 public:
  // TODO(ivankr): replace with RefCountedMemory to prevent copying.
  typedef std::vector<unsigned char> RawImage;

  // Creates a new instance from a given still frame and tries to encode raw
  // representation for it.
  // TODO(ivankr): remove eventually.
  static UserImage CreateAndEncode(const gfx::ImageSkia& image);

  // Create instance with an empty still frame and no raw data.
  UserImage();

  // Creates a new instance from a given still frame without any raw data.
  explicit UserImage(const gfx::ImageSkia& image);

  // Creates a new instance from a given still frame and raw representation.
  UserImage(const gfx::ImageSkia& image, const RawImage& raw_image);

  virtual ~UserImage();

  const gfx::ImageSkia& image() const { return image_; }

  // Optional raw representation of the still image.
  bool has_raw_image() const { return has_raw_image_; }
  const RawImage& raw_image() const { return raw_image_; }

  // Discards the stored raw image, freeing used memory.
  void DiscardRawImage();

  // URL from which this image was originally downloaded, if any.
  void set_url(const GURL& url) { url_ = url; }
  GURL url() const { return url_; }

  // Whether |raw_image| contains data in format that is considered safe to
  // decode in sensitive environment (on Login screen).
  bool is_safe_format() const { return is_safe_format_; }
  void MarkAsSafe();

  const std::string& file_path() const { return file_path_; }
  void set_file_path(const std::string& file_path) { file_path_ = file_path; }

 private:
  gfx::ImageSkia image_;
  bool has_raw_image_;
  RawImage raw_image_;
  GURL url_;

  // If image was loaded from the local file, file path is stored here.
  std::string file_path_;
  bool is_safe_format_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_IMAGE_USER_IMAGE_H_
