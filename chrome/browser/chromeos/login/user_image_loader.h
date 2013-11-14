// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/image_decoder.h"

class SkBitmap;

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {

class UserImage;

// Helper that reads, decodes and optionally resizes an image on a background
// thread. Returns the image in the form of an SkBitmap.
class UserImageLoader : public base::RefCountedThreadSafe<UserImageLoader>,
                        public ImageDecoder::Delegate {
 public:
  // Callback used to return the result of an image load operation.
  typedef base::Callback<void(const UserImage& user_image)> LoadedCallback;

  // All file I/O, decoding and resizing are done via |background_task_runner|.
  UserImageLoader(
      ImageDecoder::ImageCodec image_codec,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // Load an image in the background and call |loaded_cb| with the result. If
  // |size| is positive, the image is cropped to a square and shrunk so that it
  // does not exceed |size|x|size|. The first variant of this method reads the
  // image from |filepath| on disk, the second processes |data| read into memory
  // already.
  void Start(const std::string& filepath,
             int size,
             const LoadedCallback& loaded_cb);
  void Start(scoped_ptr<std::string> data,
             int size,
             const LoadedCallback& loaded_cb);

 private:
  friend class base::RefCountedThreadSafe<UserImageLoader>;

  // Contains attributes we need to know about each image we decode.
  struct ImageInfo {
    ImageInfo(int size, const LoadedCallback& loaded_cb);
    ~ImageInfo();

    const int size;
    const LoadedCallback loaded_cb;
  };

  typedef std::map<const ImageDecoder*, ImageInfo> ImageInfoMap;

  virtual ~UserImageLoader();

  // Reads the image from |filepath| and starts the decoding process. This
  // method may only be invoked via the |background_task_runner_|.
  void ReadAndDecodeImage(const std::string& filepath,
                          const ImageInfo& image_info);

  // Decodes the image |data|. This method may only be invoked via the
  // |background_task_runner_|.
  void DecodeImage(const scoped_ptr<std::string> data,
                   const ImageInfo& image_info);

  // ImageDecoder::Delegate implementation. These callbacks will only be invoked
  // via the |background_task_runner_|.
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

  // The foreground task runner on which |this| is instantiated, Start() is
  // called and LoadedCallbacks are invoked.
  scoped_refptr<base::SequencedTaskRunner> foreground_task_runner_;

  // The background task runner on which file I/O, image decoding and resizing
  // are done.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Specify how the file should be decoded in the utility process.
  const ImageDecoder::ImageCodec image_codec_;

  // Holds information about the images currently being decoded. Accessed via
  // |background_task_runner_| only.
  ImageInfoMap image_info_map_;

  DISALLOW_COPY_AND_ASSIGN(UserImageLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_
