// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

class Profile;

namespace extensions {

class WebcamPrivateSetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateSetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.set", WEBCAMPRIVATE_SET);

 protected:
  virtual ~WebcamPrivateSetFunction();
  virtual bool RunSync() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateSetFunction);
};

class WebcamPrivateGetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateGetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.get", WEBCAMPRIVATE_GET);

 protected:
  virtual ~WebcamPrivateGetFunction();
  virtual bool RunSync() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateGetFunction);
};

class WebcamPrivateResetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateResetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.reset", WEBCAMPRIVATE_RESET);

 protected:
  virtual ~WebcamPrivateResetFunction();
  virtual bool RunSync() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateResetFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
