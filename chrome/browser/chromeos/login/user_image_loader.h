// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_
#pragma once

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/image_decoder.h"

class MessageLoop;
class SkBitmap;

namespace chromeos {

// A facility to read a file containing user image asynchronously in the IO
// thread. Returns the image in the form of an SkBitmap.
class UserImageLoader : public base::RefCountedThreadSafe<UserImageLoader>,
                        public ImageDecoder::Delegate {
 public:
  // Callback used to inidicate that image has been loaded.
  typedef base::Callback<void(const SkBitmap& image)> LoadedCallback;

  UserImageLoader();

  // Start reading the image from |filepath| on the file thread. Calls
  // |loaded_cb| when image has been successfully loaded.
  // If |size| is positive, image is resized to |size|x|size| pixels.
  void Start(const std::string& filepath, int size,
             const LoadedCallback& loaded_cb);

 private:
  friend class base::RefCountedThreadSafe<UserImageLoader>;

  // Contains attributes we need to know about each image we decode.
  struct ImageInfo {
    ImageInfo(int size, const LoadedCallback& loaded_cb);
    ~ImageInfo();

    int size;
    LoadedCallback loaded_cb;
  };

  typedef std::map<const ImageDecoder*, ImageInfo> ImageInfoMap;

  virtual ~UserImageLoader();

  // Method that reads the file on the file thread and starts decoding it in
  // sandboxed process.
  void LoadImage(const std::string& filepath, const ImageInfo& image_info);

  // ImageDecoder::Delegate implementation.
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image);
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder);

  // The message loop object of the thread in which we notify the delegate.
  MessageLoop* target_message_loop_;

  // Holds info structures about all images we're trying to decode.
  // Accessed only on FILE thread.
  ImageInfoMap image_info_map_;

  DISALLOW_COPY_AND_ASSIGN(UserImageLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_
