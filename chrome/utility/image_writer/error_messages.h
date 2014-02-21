// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ERROR_MESSAGES_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ERROR_MESSAGES_H_

namespace image_writer {
namespace error {

extern const char kCleanUp[];
extern const char kCloseDevice[];
extern const char kCloseImage[];
extern const char kInvalidDevice[];
extern const char kNoOperationInProgress[];
extern const char kOpenDevice[];
extern const char kOpenImage[];
extern const char kOperationAlreadyInProgress[];
extern const char kReadDevice[];
extern const char kReadImage[];
extern const char kWriteImage[];
extern const char kVerificationFailed[];

}  // namespace error
}  // namespace image_writer

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ERROR_MESSAGES_H_
