// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('serial_service', [
    'content/public/renderer/service_provider',
    'device/serial/serial.mojom',
    'mojo/public/js/bindings/router',
], function(serviceProvider, serialMojom, routerModule) {

  function defineService(proxy, handle) {
    if (!handle)
      handle = serviceProvider.connectToService(proxy.NAME_);
    var router = new routerModule.Router(handle);
    var service = new proxy(router);
    return {
      service: service,
      router: router,
    };
  }

  var service = defineService(serialMojom.SerialServiceProxy).service;

  function getDevices() {
    return service.getDevices().then(function(response) {
      return $Array.map(response.devices, function(device) {
        var result = {path: device.path || ''};
        if (device.has_vendor_id)
          result.vendorId = device.vendor_id;
        if (device.has_product_id)
          result.productId = device.product_id;
        if (device.display_name)
          result.displayName = device.display_name;
        return result;
      });
    });
  }

  return {
    getDevices: getDevices,
  };
});
