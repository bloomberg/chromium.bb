// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/chromeos/login/image_decoder.h"

class MessageLoop;
class SkBitmap;

namespace chromeos {

// A facility to read a file containing user image asynchronously in the IO
// thread. Returns the image in the form of an SkBitmap.
class UserImageLoader : public base::RefCountedThreadSafe<UserImageLoader>,
                        public ImageDecoder::Delegate {
 public:
  class Delegate {
   public:
    // Invoked when user image has been read.
    // |should_save_image| indicates if user image should be saved somewhere
    // for later use.
    virtual void OnImageLoaded(const std::string& username,
                               const SkBitmap& image,
                               bool should_save_image) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit UserImageLoader(Delegate* delegate);

  // Start reading the image for |username| from |filepath| on the file thread.
  // |should_save_image| is passed to OnImageLoaded handler.
  void Start(const std::string& username,
             const std::string& filepath,
             bool should_save_image);

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  friend class base::RefCountedThreadSafe<UserImageLoader>;

  // Contains attributes we need to know about each image we decode.
  struct ImageInfo {
    ImageInfo(const std::string& username, bool should_save)
        : username(username),
          should_save_image(should_save) {
    }

    std::string username;
    bool should_save_image;
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

  // Notifies the delegate that image was loaded, on delegate's thread.
  void NotifyDelegate(const SkBitmap& image, const ImageInfo& image_info);

  // The message loop object of the thread in which we notify the delegate.
  MessageLoop* target_message_loop_;

  // Delegate to notify about finishing the load of the image.
  Delegate* delegate_;

  // Holds info structures about all images we're trying to decode.
  // Accessed only on FILE thread.
  ImageInfoMap image_info_map_;

  DISALLOW_COPY_AND_ASSIGN(UserImageLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_LOADER_H_
