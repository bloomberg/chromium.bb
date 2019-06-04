// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_POLICY_ALLOWED_DEVICES_H_
#define CHROME_BROWSER_USB_USB_POLICY_ALLOWED_DEVICES_H_

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/optional.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/origin.h"

namespace base {
class Value;
}  // namespace base

namespace device {
namespace mojom {
class UsbDeviceInfo;
}  // namespace mojom
}  // namespace device

class PrefService;

// This class is used to initialize a UsbDeviceIdsToUrlsMap from the
// preference value for the WebUsbAllowDevicesForUrls and
// DeviceWebUsbAllowDevicesForUrls policy. The map provides an efficient method
// of checking if a particular device is allowed to be used by the given
// requesting and embedding origins. Additionally, this class also uses
// PrefChangeRegistrars to observe for changes to the preference values so that
// the map can be updated accordingly.
class UsbPolicyAllowedDevices {
 public:
  // A map of device IDs to a set of origins stored in a std::pair. The device
  // IDs correspond to a pair of |vendor_id| and |product_id| integers. The
  // origins correspond to a pair of |requesting_origin| and |embedding_origin|
  // that are allowed to access the device mapped to them. If |embedding_origin|
  // is base::nullopt then |requesting_origin| is allowed to access the device
  // when embedded in any top-level frame.
  using UsbDeviceIdsToUrlsMap =
      std::map<std::pair<int, int>,
               std::set<std::pair<url::Origin, base::Optional<url::Origin>>>>;

  // Initializes PrefChangeRegistrars and adds observers to listen to the prefs
  // controlled by user and device policy.
  explicit UsbPolicyAllowedDevices(PrefService* profile_prefs,
                                   PrefService* local_state_prefs);
  ~UsbPolicyAllowedDevices();

  // Checks if |requesting_origin| (when embedded within |embedding_origin|) is
  // allowed to use the device with |device_info|.
  bool IsDeviceAllowed(const url::Origin& requesting_origin,
                       const url::Origin& embedding_origin,
                       const device::mojom::UsbDeviceInfo& device_info);
  bool IsDeviceAllowed(const url::Origin& requesting_origin,
                       const url::Origin& embedding_origin,
                       const std::pair<int, int>& device_ids);

  const UsbDeviceIdsToUrlsMap& map() const { return usb_device_ids_to_urls_; }

 private:
  // Creates or updates the |usb_device_ids_to_urls_| map using the prefs
  // controlled by user and device policy. The existing map is cleared to ensure
  // that previous pref settings are removed.
  void CreateOrUpdateMap();
  void CreateOrUpdateMapFromPrefValue(const base::Value* pref_value);

  // Allow for this class to observe changes to the pref value.
  PrefChangeRegistrar profile_pref_change_registrar_;
#if defined(OS_CHROMEOS)
  PrefChangeRegistrar local_state_pref_change_registrar_;
#endif  // defined(OS_CHROMEOS)

  UsbDeviceIdsToUrlsMap usb_device_ids_to_urls_;
};

#endif  // CHROME_BROWSER_USB_USB_POLICY_ALLOWED_DEVICES_H_
