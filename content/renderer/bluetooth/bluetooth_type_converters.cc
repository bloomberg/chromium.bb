// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/bluetooth_type_converters.h"

#include "content/child/mojo/type_converters.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

namespace mojo {

// static
blink::mojom::WebBluetoothScanFilterPtr TypeConverter<
    blink::mojom::WebBluetoothScanFilterPtr,
    blink::WebBluetoothScanFilter>::Convert(const blink::WebBluetoothScanFilter&
                                                web_filter) {
  blink::mojom::WebBluetoothScanFilterPtr filter =
      blink::mojom::WebBluetoothScanFilter::New();
  if (!web_filter.services.isEmpty())
    filter->services = Array<String>::From(web_filter.services);

  if (web_filter.hasName)
    filter->name = String::From(web_filter.name);

  if (!web_filter.namePrefix.isEmpty())
    filter->name_prefix = String::From(web_filter.namePrefix);
  return filter;
}

blink::mojom::WebBluetoothRequestDeviceOptionsPtr
TypeConverter<blink::mojom::WebBluetoothRequestDeviceOptionsPtr,
              blink::WebRequestDeviceOptions>::
    Convert(const blink::WebRequestDeviceOptions& web_options) {
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();

  options->filters = mojo::Array<blink::mojom::WebBluetoothScanFilterPtr>::From(
      web_options.filters);
  options->optional_services =
      mojo::Array<mojo::String>::From(web_options.optionalServices);
  return options;
}

}  // namespace mojo
