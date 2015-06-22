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
struct TypeConverter<blink::WebVRVector3, content::VRVector3Ptr> {
  static blink::WebVRVector3 Convert(const content::VRVector3Ptr& input);
};

template <>
struct TypeConverter<blink::WebVRVector4, content::VRVector4Ptr> {
  static blink::WebVRVector4 Convert(const content::VRVector4Ptr& input);
};

template <>
struct TypeConverter<blink::WebVRRect, content::VRRectPtr> {
  static blink::WebVRRect Convert(const content::VRRectPtr& input);
};

template <>
struct TypeConverter<blink::WebVRFieldOfView, content::VRFieldOfViewPtr> {
  static blink::WebVRFieldOfView Convert(
      const content::VRFieldOfViewPtr& input);
};

template <>
struct TypeConverter<blink::WebVREyeParameters, content::VREyeParametersPtr> {
  static blink::WebVREyeParameters Convert(
      const content::VREyeParametersPtr& input);
};

template <>
struct TypeConverter<blink::WebVRHMDInfo, content::VRHMDInfoPtr> {
  static blink::WebVRHMDInfo Convert(const content::VRHMDInfoPtr& input);
};

template <>
struct TypeConverter<blink::WebVRDevice, content::VRDeviceInfoPtr> {
  static blink::WebVRDevice Convert(const content::VRDeviceInfoPtr& input);
};

template <>
struct TypeConverter<blink::WebHMDSensorState, content::VRSensorStatePtr> {
  static blink::WebHMDSensorState Convert(
      const content::VRSensorStatePtr& input);
};

}  // namespace mojo

#endif  // CHROME_CONTENT_RENDERER_VR_VR_TYPE_CONVERTERS_H_