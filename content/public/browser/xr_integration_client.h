// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_XR_INTEGRATION_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_XR_INTEGRATION_CLIENT_H_

#include <memory>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"

#if !defined(OS_ANDROID)
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#endif

namespace device {
class VRDeviceProvider;
}

namespace content {
class XrConsentHelper;
class XrInstallHelper;

using XRProviderList = std::vector<std::unique_ptr<device::VRDeviceProvider>>;
#if !defined(OS_ANDROID)
// This class is intended to provide implementers a means of accessing the
// the XRCompositorHost returned from a create session call. Content has no
// obligation to notify it of any events (other than any observers that
// implementers subscribe to independently). Any VrUiHost created this way is
// guaranteed to be kept alive until the device that it was created for has been
// removed. Note that currently this is only supported on Windows.
class CONTENT_EXPORT VrUiHost {
 public:
  virtual ~VrUiHost() = default;
};
#endif

// A helper class for |ContentBrowserClient| to wrap for XR-specific
// integration that may be needed from content/. Currently it only provides
// access to relevant XrInstallHelpers, but will eventually be expanded to
// include integration points for VrUiHost.
// This should be implemented by embedders.
class CONTENT_EXPORT XrIntegrationClient {
 public:
  XrIntegrationClient() = default;
  virtual ~XrIntegrationClient() = default;
  XrIntegrationClient(const XrIntegrationClient&) = delete;
  XrIntegrationClient& operator=(const XrIntegrationClient&) = delete;

  // Returns the |XrInstallHelper| for the corresponding |XRDeviceId|, or
  // nullptr if the requested |XRDeviceId| does not have any required extra
  // installation steps.
  virtual std::unique_ptr<XrInstallHelper> GetInstallHelper(
      device::mojom::XRDeviceId device_id);

  // Returns the |XrConsentHelper| for the corresponding |XRDeviceId|, or
  // nullptr if the requested |XRDeviceId| cannot prompt for consent.
  // In this case, consent is assumed to have been denied.
  virtual std::unique_ptr<XrConsentHelper> GetConsentHelper(
      device::mojom::XRDeviceId device_id);

  // Returns a vector of device providers that should be used in addition to
  // any default providers built-in to //content.
  virtual XRProviderList GetAdditionalProviders();

#if !defined(OS_ANDROID)
  // Creates a VrUiHost object for the specified device_id, and takes ownership
  // of any XRCompositor supplied from the runtime.
  virtual std::unique_ptr<VrUiHost> CreateVrUiHost(
      device::mojom::XRDeviceId device_id,
      mojo::PendingRemote<device::mojom::XRCompositorHost> compositor);
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_XR_INTEGRATION_CLIENT_H_
