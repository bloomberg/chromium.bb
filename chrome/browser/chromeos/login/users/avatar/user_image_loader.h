// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/image_decoder.h"

class SkBitmap;

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
      ImageDecoder::ImageCodec image_codec,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // Load an image in the background and call |loaded_cb| with the resulting
  // UserImage (which may be empty in case of error). If |pixels_per_side| is
  // positive, the image is cropped to a square and shrunk so that it does not
  // exceed |pixels_per_side|x|pixels_per_side|. The first variant of this
  // method reads the image from |filepath| on disk, the second processes |data|
  // read into memory already.
  void Start(const std::string& filepath,
             int pixels_per_side,
             const LoadedCallback& loaded_cb);
  void Start(scoped_ptr<std::string> data,
             int pixels_per_side,
             const LoadedCallback& loaded_cb);

 private:
  friend class base::RefCountedThreadSafe<UserImageLoader>;

  // Contains attributes we need to know about each image we decode.
  struct ImageInfo {
    ImageInfo(const std::string& file_path,
              int pixels_per_side,
              const LoadedCallback& loaded_cb);
    ~ImageInfo();

    const std::string file_path;
    const int pixels_per_side;
    const LoadedCallback loaded_cb;
  };

  class UserImageRequest : public ImageDecoder::ImageRequest {
   public:
    UserImageRequest(const ImageInfo& image_info,
                     const std::string& image_data,
                     const scoped_refptr<UserImageLoader>& user_image_loader);

    // ImageDecoder::ImageRequest implementation. These callbacks will only be
    // invoked via user_image_loader_'s background_task_runner_.
    void OnImageDecoded(const SkBitmap& decoded_image) override;
    void OnDecodeImageFailed() override;

   private:
    ~UserImageRequest() override;

    const ImageInfo image_info_;
    std::vector<unsigned char> image_data_;
    scoped_refptr<UserImageLoader> user_image_loader_;
  };

  ~UserImageLoader();

  // Reads the image from |image_info.file_path| and starts the decoding
  // process. This method may only be invoked via the |background_task_runner_|.
  void ReadAndDecodeImage(const ImageInfo& image_info);

  // Decodes the image |data|. This method may only be invoked via the
  // |background_task_runner_|.
  void DecodeImage(const scoped_ptr<std::string> data,
                   const ImageInfo& image_info);

  // The foreground task runner on which |this| is instantiated, Start() is
  // called and LoadedCallbacks are invoked.
  scoped_refptr<base::SequencedTaskRunner> foreground_task_runner_;

  // The background task runner on which file I/O, image decoding and resizing
  // are done.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Specify how the file should be decoded in the utility process.
  const ImageDecoder::ImageCodec image_codec_;

  DISALLOW_COPY_AND_ASSIGN(UserImageLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_LOADER_H_
