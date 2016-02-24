// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/image_decoder.h"

namespace base {
class SequencedTaskRunner;
}

namespace user_manager {
class UserImage;
}

namespace chromeos {

// Helper that reads, decodes and optionally resizes an image on a background
// thread. Returns the image in the form of an SkBitmap.
class UserImageLoader : public base::RefCountedThreadSafe<UserImageLoader> {
 public:
  // Callback used to return the result of an image load operation.
  typedef base::Callback<void(const user_manager::UserImage& user_image)>
      LoadedCallback;

  // All file I/O, decoding and resizing are done via |background_task_runner|.
  UserImageLoader(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // Load an image with |image_codec| in the background and call |loaded_cb|
  // with the resulting UserImage (which may be empty in case of error). If
  // |pixels_per_side| is positive, the image is cropped to a square and
  // shrunk so that it does not exceed
  // |pixels_per_side|x|pixels_per_side|. The first variant of this method
  // reads the image from |file_path| on disk, the second processes |data|
  // read into memory already.
  void StartWithFilePath(const base::FilePath& file_path,
                         ImageDecoder::ImageCodec image_codec,
                         int pixels_per_side,
                         const LoadedCallback& loaded_cb);
  void StartWithData(scoped_ptr<std::string> data,
                     ImageDecoder::ImageCodec image_codec,
                     int pixels_per_side,
                     const LoadedCallback& loaded_cb);

 private:
  friend class base::RefCountedThreadSafe<UserImageLoader>;
  struct ImageInfo;
  class UserImageRequest;

  ~UserImageLoader();

  // Decodes the image |data| if |data_is_ready| is true.
  void DecodeImage(const ImageInfo& image_info,
                   const std::string* data,
                   bool data_is_ready);

  // The foreground task runner on which |this| is instantiated, Start() is
  // called and LoadedCallbacks are invoked.
  scoped_refptr<base::SequencedTaskRunner> foreground_task_runner_;

  // The background task runner on which file I/O, image decoding and resizing
  // are done.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UserImageLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_
