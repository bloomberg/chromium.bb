// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_AUDIO_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_AUDIO_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/media/media_devices.mojom.h"
#include "content/renderer/media/media_stream_constraints_util.h"

namespace blink {
class WebMediaConstraints;
}

namespace content {

using AudioDeviceCaptureCapabilities =
    std::vector<::mojom::AudioInputDeviceCapabilitiesPtr>;

// This function implements the SelectSettings algorithm for audio tracks as
// described in https://w3c.github.io/mediacapture-main/#dfn-selectsettings
// The algorithm starts with a set containing all possible candidate settings
// based on hardware capabilities (passed via the |capabilities| parameter) and
// supported values for properties not involved in device selection. Candidates
// that do not support the basic constraint set from |constraints| are removed.
// If the set of candidates is empty after this step, the function returns an
// AudioCaptureSettings object without value and whose failed_constraint_name()
// method returns the name of one of the (possibly many) constraints that could
// not be satisfied or an empty string if the set of candidates was initially
// empty (e.g., if there are no devices in the system).
// After the basic constraint set is applied, advanced constraint sets are
// applied. If no candidates can satisfy an advanced set, the advanced set is
// ignored, otherwise the candidates that cannot satisfy the advanced set are
// removed.
// Once all constraint sets are applied, the result is selected from the
// remaining candidates by giving preference to candidates closest to the ideal
// values specified in the basic constraint set, or using default
// implementation-specific values.
// The result includes the following properties:
//  * Device. A device can be chosen using the device_id constraint.
//    For device capture, the validity of device IDs is checked by
//    SelectSettings since the list of allowed device IDs is known in advance.
//    For content capture, all device IDs are considered valid by
//    SelectSettings. Actual validation is performed by the getUserMedia
//    implementation.
//  * Audio features: the hotword_enabled, disable_local_echo and
//    render_to_associated_sink constraints can be used to enable the
//    corresponding audio feature. If not specified, their default value is
//    false, except for disable_local_echo, whose default value is false only
//    for desktop capture.
//  * Audio processing. The remaining constraints are used to control audio
//    processing. This is how audio-processing properties are set for device
//    capture(see the content::AudioProcessingProperties struct) :
//      - enable_sw_echo_cancellation: If the selected device has hardware echo
//        cancellation, software echo cancellation is disabled regardless of
//        any constraint values. Otherwise, it is enabled by default unless
//        either the echo_cancellation or the goog_echo_cancellation constraint
//        has a final value of false after applying all constraint sets. Note
//        that if these constraints have contradictory values, SelectSettings
//        fails and returns no value.
//      - disable_hw_echo_cancellation: For devices that have hardware echo
//        cancellation, the feature is disabled if the echo_cancellation or
//        goog_echo_cancellation constraints have a final value of false.
//      - goog_audio_mirroring: This property is mapped directly from the final
//        value of the goog_audio_mirroring constraint. If no value is
//        explicitly specified, the default value is false.
//    The remaining audio-processing properties are directly mapped from the
//    final value of the corresponding constraints. If no value is explicitly
//    specified, the default value is the same as the final value of the
//    echo_cancellation constraint.  If the echo_cancellation constraint is
//    not explicitly specified, the default value is implementation defined
//    (see content::AudioProcessingProperties).
//    For content capture the rules are the same, but all properties are false
//    by default, regardless of the value of the echo_cancellation constraint.
//    Note that it is important to distinguish between audio properties and
//    constraints. Constraints are an input to SelectSettings, while properties
//    are part of the output. The value for most boolean properties comes
//    directly from a corresponding boolean constraint, but this is not true for
//    all constraints and properties. For example, the echo_cancellation and
//    goog_echo_cancellation constraints  are not directly mapped to any
//    property, but they, together with hardware characteristics, influence the
//    enabling and disabling of software and hardware echo cancellation.
//    Moreover, the echo_cancellation constraint influences most other
//    audio-processing properties for which no explicit value is provided in
//    their corresponding constraints.
// TODO(guidou): Add support for other standard constraints such as sampleRate,
// channelCount and groupId. http://crbug.com/731170
AudioCaptureSettings CONTENT_EXPORT
SelectSettingsAudioCapture(const AudioDeviceCaptureCapabilities& capabilities,
                           const blink::WebMediaConstraints& constraints);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_AUDIO_H_
