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
#include "chrome/browser/vr/test/mock_xr_session_request_consent_manager.h"
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

#if defined(OS_WIN)
  MockXRSessionRequestConsentManager consent_manager_;
#endif
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

// OpenVR and WMR feature only defined on Windows.
#ifdef OS_WIN
// OpenVR-specific subclass of WebXrVrBrowserTestBase.
class WebXrVrOpenVrBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  WebXrVrOpenVrBrowserTestBase() {
    enable_features_.push_back(features::kOpenVR);
  }
  XrBrowserTestBase::RuntimeType GetRuntimeType() const override;
};

// WMR-specific subclass of WebXrVrBrowserTestBase.
class WebXrVrWMRBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  // WMR enabled by default, so no need to add anything in the constructor.
  XrBrowserTestBase::RuntimeType GetRuntimeType() const override;
};

// Test class with standard features enabled: WebXR and OpenVR.
class WebXrVrBrowserTestStandard : public WebXrVrOpenVrBrowserTestBase {
 public:
  WebXrVrBrowserTestStandard() {
    enable_features_.push_back(features::kWebXr);

    runtime_requirements_.push_back(XrTestRequirement::DIRECTX_11_1);

#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};

class WebXrVrBrowserTestWMR : public WebXrVrWMRBrowserTestBase {
 public:
  WebXrVrBrowserTestWMR() {
    // WMR already enabled by default.
    enable_features_.push_back(features::kWebXr);
  }
};

// Test class with WebXR disabled.
class WebXrVrBrowserTestWebXrDisabled : public WebXrVrOpenVrBrowserTestBase {
 public:
  WebXrVrBrowserTestWebXrDisabled() {
#if BUILDFLAG(ENABLE_WINDOWS_MR)
    disable_features_.push_back(features::kWindowsMixedReality);
#endif
  }
};
#endif  // OS_WIN

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_
