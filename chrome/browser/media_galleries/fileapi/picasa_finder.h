// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FINDER_H_

#include <string>

#include "base/callback.h"

namespace picasa {

namespace PicasaFinder {

typedef base::Callback<void(const std::string&)> DeviceIDCallback;

// Bounces to FILE thread to find Picasa database. If the platform supports
// Picasa and a Picasa database is found, |callback| will be invoked on the
// calling thread. Otherwise, |callback| is not invoked.
void FindPicasaDatabase(const DeviceIDCallback& callback);

}

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FINDER_H_
