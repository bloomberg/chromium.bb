// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

class Profile;

namespace extensions {

class WebcamPrivateSetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateSetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.set", WEBCAMPRIVATE_SET);

 protected:
  ~WebcamPrivateSetFunction() override;
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateSetFunction);
};

class WebcamPrivateGetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateGetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.get", WEBCAMPRIVATE_GET);

 protected:
  ~WebcamPrivateGetFunction() override;
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateGetFunction);
};

class WebcamPrivateResetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateResetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.reset", WEBCAMPRIVATE_RESET);

 protected:
  ~WebcamPrivateResetFunction() override;
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateResetFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
