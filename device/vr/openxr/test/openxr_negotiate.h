// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_TEST_OPENXR_NEGOTIATE_H_
#define DEVICE_VR_OPENXR_TEST_OPENXR_NEGOTIATE_H_

#include <d3d11.h>
#include <unknwn.h>

#include "third_party/openxr/include/openxr/loader_interfaces.h"
#include "third_party/openxr/include/openxr/openxr.h"
#include "third_party/openxr/include/openxr/openxr_platform.h"

// This file contains functions that are used by the openxr_loader.dll to call
// into the fake OpenXR Runtime. Used for testing purposes only, so this should
// only be used to call the fake OpenXR APIs defined in
// fake_openxr_impl_api.cc.

// Please add new OpenXR APIs below in alphabetical order.
XrResult GetInstanceProcAddress(XrInstance instance,
                                const char* name,
                                PFN_xrVoidFunction* function) {
  if (strcmp(name, "xrAcquireSwapchainImage") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrAcquireSwapchainImage);
  } else if (strcmp(name, "xrBeginFrame") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrBeginFrame);
  } else if (strcmp(name, "xrBeginSession") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrBeginSession);
  } else if (strcmp(name, "xrCreateInstance") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateInstance);
  } else if (strcmp(name, "xrCreateReferenceSpace") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateReferenceSpace);
  } else if (strcmp(name, "xrCreateSession") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateSession);
  } else if (strcmp(name, "xrCreateSwapchain") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrCreateSwapchain);
  } else if (strcmp(name, "xrDestroyInstance") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrDestroyInstance);
  } else if (strcmp(name, "xrEndFrame") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrEndFrame);
  } else if (strcmp(name, "xrEndSession") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrEndSession);
  } else if (strcmp(name, "xrEnumerateEnvironmentBlendModes") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateEnvironmentBlendModes);
  } else if (strcmp(name, "xrEnumerateInstanceExtensionProperties") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(
        xrEnumerateInstanceExtensionProperties);
  } else if (strcmp(name, "xrEnumerateSwapchainImages") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateSwapchainImages);
  } else if (strcmp(name, "xrEnumerateViewConfigurationViews") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrEnumerateViewConfigurationViews);
  } else if (strcmp(name, "xrGetD3D11GraphicsRequirementsKHR") == 0) {
    *function =
        reinterpret_cast<PFN_xrVoidFunction>(xrGetD3D11GraphicsRequirementsKHR);
  } else if (strcmp(name, "xrGetSystem") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrGetSystem);
  } else if (strcmp(name, "xrLocateSpace") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrLocateSpace);
  } else if (strcmp(name, "xrLocateViews") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrLocateViews);
  } else if (strcmp(name, "xrReleaseSwapchainImage") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrReleaseSwapchainImage);
  } else if (strcmp(name, "xrWaitFrame") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrWaitFrame);
  } else if (strcmp(name, "xrWaitSwapchainImage") == 0) {
    *function = reinterpret_cast<PFN_xrVoidFunction>(xrWaitSwapchainImage);
  } else {
    return XR_ERROR_FUNCTION_UNSUPPORTED;
  }

  return XR_SUCCESS;
}

// The single exported function in fake OpenXR Runtime DLL which the OpenXR
// loader calls for negotiation. GetInstanceProcAddress is returned to the
// loader, which is then used by the loader to call OpenXR APIs.
// extern "C" is needed because the OpenXR Loader expects the name of this
// function to be unmangled. The real OpenXR runtime does this as well.
extern "C" __declspec(dllexport) XrResult xrNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo,
    XrNegotiateRuntimeRequest* runtimeRequest) {
  runtimeRequest->runtimeInterfaceVersion = 1;
  runtimeRequest->runtimeXrVersion = XR_MAKE_VERSION(0, 1, 0);
  runtimeRequest->getInstanceProcAddr = GetInstanceProcAddress;

  return XR_SUCCESS;
}

#endif  // DEVICE_VR_OPENXR_TEST_OPENXR_NEGOTIATE_H_
