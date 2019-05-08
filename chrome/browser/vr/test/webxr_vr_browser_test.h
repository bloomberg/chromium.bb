// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_

#include "build/build_config.h"
#include "chrome/browser/vr/test/conditional_skipping.h"
#include "chrome/browser/vr/test/webxr_browser_test.h"
#include "chrome/browser/vr/test/xr_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "device/base/features.h"
#include "device/vr/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "services/service_manager/sandbox/features.h"
#endif

namespace vr {

// WebXR for VR-specific test base class.
class WebXrVrBrowserTestBase : public WebXrBrowserTestBase {
 public:
  void EnterSessionWithUserGesture(content::WebContents* web_contents) override;
  void EnterSessionWithUserGestureOrFail(
      content::WebContents* web_contents) override;
  void EndSession(content::WebContents* web_contents) override;
  void EndSessionOrFail(content::WebContents* web_contents) override;

  // Necessary to use the WebContents-less versions of functions.
  using WebXrBrowserTestBase::XrDeviceFound;
  using WebXrBrowserTestBase::EnterSessionWithUserGesture;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureOrFail;
  using WebXrBrowserTestBase::EndSession;
  using WebXrBrowserTestBase::EndSessionOrFail;
};

// Test class with OpenVR disabled.
class WebXrVrBrowserTestOpenVrDisabled : public WebXrVrBrowserTestBase {
 public:
  WebXrVrBrowserTestOpenVrDisabled() {
    enable_features_.push_back(features::kWebXr);

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};

// WebXrOrientationSensorDevice is only defined when the enable_vr flag is set.
#if BUILDFLAG(ENABLE_VR)
class WebXrVrBrowserTestSensorless : public WebXrVrBrowserTestBase {
 public:
  WebXrVrBrowserTestSensorless() {
    enable_features_.push_back(features::kWebXr);
    disable_features_.push_back(device::kWebXrOrientationSensorDevice);

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};
#endif

// OpenVR feature only defined on Windows.
#ifdef OS_WIN
// Test class with standard features enabled: WebXR and OpenVR.
class WebXrVrBrowserTestStandard : public WebXrVrBrowserTestBase {
 public:
  WebXrVrBrowserTestStandard() {
    enable_features_.push_back(features::kOpenVR);
    enable_features_.push_back(features::kWebXr);

    runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};

// Test class with WebXR disabled.
class WebXrVrBrowserTestWebXrDisabled : public WebXrVrBrowserTestBase {
 public:
  WebXrVrBrowserTestWebXrDisabled() {
    enable_features_.push_back(features::kOpenVR);

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};
#endif  // OS_WIN

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_
