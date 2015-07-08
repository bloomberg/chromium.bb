// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/web/WebGeolocationClient.h"

namespace html_viewer {

class GeolocationClientImpl : public blink::WebGeolocationClient {
 public:
  GeolocationClientImpl();
  ~GeolocationClientImpl() override;

  // WebGeolocationClient methods:
  void startUpdating() override;
  void stopUpdating() override;
  void setEnableHighAccuracy(bool) override;
  bool lastPosition(blink::WebGeolocationPosition&) override;
  void requestPermission(
      const blink::WebGeolocationPermissionRequest&) override;
  void cancelPermissionRequest(
      const blink::WebGeolocationPermissionRequest&) override;
  void setController(blink::WebGeolocationController* controller) override;

 private:
  scoped_ptr<blink::WebGeolocationController> controller_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationClientImpl);
};

}  // namespace html_viewer
