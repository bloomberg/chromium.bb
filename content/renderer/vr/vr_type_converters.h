// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CONTENT_RENDERER_VR_VR_TYPE_CONVERTERS_H_
#define CHROME_CONTENT_RENDERER_VR_VR_TYPE_CONVERTERS_H_

#include "content/common/vr_service.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "third_party/WebKit/public/platform/modules/vr/WebVR.h"

namespace mojo {

// Type/enum conversions from WebVR data types to Mojo data types
// and vice versa.

template <>
struct TypeConverter<blink::WebVRVector3, content::mojom::VRVector3Ptr> {
  static blink::WebVRVector3 Convert(const content::mojom::VRVector3Ptr& input);
};

template <>
struct TypeConverter<blink::WebVRVector4, content::mojom::VRVector4Ptr> {
  static blink::WebVRVector4 Convert(const content::mojom::VRVector4Ptr& input);
};

template <>
struct TypeConverter<blink::WebVRRect, content::mojom::VRRectPtr> {
  static blink::WebVRRect Convert(const content::mojom::VRRectPtr& input);
};

template <>
struct TypeConverter<blink::WebVRFieldOfView,
                     content::mojom::VRFieldOfViewPtr> {
  static blink::WebVRFieldOfView Convert(
      const content::mojom::VRFieldOfViewPtr& input);
};

template <>
struct TypeConverter<blink::WebVREyeParameters,
                     content::mojom::VREyeParametersPtr> {
  static blink::WebVREyeParameters Convert(
      const content::mojom::VREyeParametersPtr& input);
};

template <>
struct TypeConverter<blink::WebVRHMDInfo, content::mojom::VRHMDInfoPtr> {
  static blink::WebVRHMDInfo Convert(const content::mojom::VRHMDInfoPtr& input);
};

template <>
struct TypeConverter<blink::WebVRDevice, content::mojom::VRDeviceInfoPtr> {
  static blink::WebVRDevice Convert(
      const content::mojom::VRDeviceInfoPtr& input);
};

template <>
struct TypeConverter<blink::WebHMDSensorState,
                     content::mojom::VRSensorStatePtr> {
  static blink::WebHMDSensorState Convert(
      const content::mojom::VRSensorStatePtr& input);
};

}  // namespace mojo

#endif  // CHROME_CONTENT_RENDERER_VR_VR_TYPE_CONVERTERS_H_