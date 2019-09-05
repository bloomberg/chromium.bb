// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "media" command-line switches.

#ifndef MEDIA_BASE_MEDIA_SWITCHES_H_
#define MEDIA_BASE_MEDIA_SWITCHES_H_

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/media_buildflags.h"

namespace base {
class CommandLine;
}

namespace switches {

MEDIA_EXPORT extern const char kAudioBufferSize[];

MEDIA_EXPORT extern const char kAudioServiceQuitTimeoutMs[];

MEDIA_EXPORT extern const char kAutoplayPolicy[];

MEDIA_EXPORT extern const char kDisableAudioOutput[];
MEDIA_EXPORT extern const char kFailAudioStreamCreation[];

MEDIA_EXPORT extern const char kVideoThreads[];

// TODO(crbug.com/867146): remove these switches.
MEDIA_EXPORT extern const char kEnableMediaSuspend[];
MEDIA_EXPORT extern const char kDisableMediaSuspend[];

MEDIA_EXPORT extern const char kReportVp9AsAnUnsupportedMimeType[];

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
MEDIA_EXPORT extern const char kAlsaInputDevice[];
MEDIA_EXPORT extern const char kAlsaOutputDevice[];
#endif

#if defined(OS_WIN)
MEDIA_EXPORT extern const char kEnableExclusiveAudio[];
MEDIA_EXPORT extern const char kForceWaveAudio[];
MEDIA_EXPORT extern const char kTrySupportedChannelLayouts[];
MEDIA_EXPORT extern const char kWaveOutBuffers[];
#endif

#if defined(USE_CRAS)
MEDIA_EXPORT extern const char kUseCras[];
#endif

MEDIA_EXPORT extern const char
    kUnsafelyAllowProtectedMediaIdentifierForDomain[];

MEDIA_EXPORT extern const char kUseFakeDeviceForMediaStream[];
MEDIA_EXPORT extern const char kUseFileForFakeVideoCapture[];
MEDIA_EXPORT extern const char kUseFileForFakeAudioCapture[];
MEDIA_EXPORT extern const char kUseFakeMjpegDecodeAccelerator[];
MEDIA_EXPORT extern const char kDisableAcceleratedMjpegDecode[];

MEDIA_EXPORT extern const char kRequireAudioHardwareForTesting[];
MEDIA_EXPORT extern const char kMuteAudio[];

MEDIA_EXPORT extern const char kVideoUnderflowThresholdMs[];

MEDIA_EXPORT extern const char kDisableRTCSmoothnessAlgorithm[];

MEDIA_EXPORT extern const char kForceVideoOverlays[];

MEDIA_EXPORT extern const char kMSEAudioBufferSizeLimitMb[];
MEDIA_EXPORT extern const char kMSEVideoBufferSizeLimitMb[];

MEDIA_EXPORT extern const char kClearKeyCdmPathForTesting[];
MEDIA_EXPORT extern const char kOverrideEnabledCdmInterfaceVersion[];
MEDIA_EXPORT extern const char kOverrideHardwareSecureCodecsForTesting[];

namespace autoplay {

MEDIA_EXPORT extern const char kDocumentUserActivationRequiredPolicy[];
MEDIA_EXPORT extern const char kNoUserGestureRequiredPolicy[];
MEDIA_EXPORT extern const char kUserGestureRequiredPolicy[];

}  // namespace autoplay

}  // namespace switches

namespace media {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

MEDIA_EXPORT extern const base::Feature kAudioFocusDuckFlash;
MEDIA_EXPORT extern const base::Feature kAudioFocusLossSuspendMediaSession;
MEDIA_EXPORT extern const base::Feature kAutoplayIgnoreWebAudio;
MEDIA_EXPORT extern const base::Feature kAutoplayDisableSettings;
MEDIA_EXPORT extern const base::Feature kAutoplayWhitelistSettings;
MEDIA_EXPORT extern const base::Feature kBackgroundVideoPauseOptimization;
MEDIA_EXPORT extern const base::Feature kD3D11LimitTo11_0;
MEDIA_EXPORT extern const base::Feature kD3D11PrintCodecOnCrash;
MEDIA_EXPORT extern const base::Feature kD3D11VideoDecoder;
MEDIA_EXPORT extern const base::Feature kD3D11VideoDecoderIgnoreWorkarounds;
MEDIA_EXPORT extern const base::Feature kExternalClearKeyForTesting;
MEDIA_EXPORT extern const base::Feature kFFmpegDecodeOpaqueVP8;
MEDIA_EXPORT extern const base::Feature kFailUrlProvisionFetcherForTesting;
MEDIA_EXPORT extern const base::Feature kFallbackAfterDecodeError;
MEDIA_EXPORT extern const base::Feature kGlobalMediaControls;
MEDIA_EXPORT extern const base::Feature kHardwareMediaKeyHandling;
MEDIA_EXPORT extern const base::Feature kHardwareSecureDecryption;
MEDIA_EXPORT extern const base::Feature kInternalMediaSession;
MEDIA_EXPORT extern const base::Feature kLowDelayVideoRenderingOnLiveStream;
MEDIA_EXPORT extern const base::Feature kMediaCapabilitiesWithParameters;
MEDIA_EXPORT extern const base::Feature kMediaCastOverlayButton;
MEDIA_EXPORT extern const base::Feature kMediaEngagementBypassAutoplayPolicies;
MEDIA_EXPORT extern const base::Feature kMediaEngagementHTTPSOnly;
MEDIA_EXPORT extern const base::Feature kMediaInspectorLogging;
MEDIA_EXPORT extern const base::Feature kMediaLearningExperiment;
MEDIA_EXPORT extern const base::Feature kMediaLearningFramework;
MEDIA_EXPORT extern const base::Feature kMemoryPressureBasedSourceBufferGC;
MEDIA_EXPORT extern const base::Feature kChromeosVideoDecoder;
MEDIA_EXPORT extern const base::Feature kNewEncodeCpuLoadEstimator;
MEDIA_EXPORT extern const base::Feature kOverflowIconsForMediaControls;
MEDIA_EXPORT extern const base::Feature kOverlayFullscreenVideo;
MEDIA_EXPORT extern const base::Feature kPictureInPicture;
MEDIA_EXPORT extern const base::Feature kPreloadMediaEngagementData;
MEDIA_EXPORT extern const base::Feature kPreloadMetadataLazyLoad;
MEDIA_EXPORT extern const base::Feature kPreloadMetadataSuspend;
MEDIA_EXPORT extern const base::Feature kRecordMediaEngagementScores;
MEDIA_EXPORT extern const base::Feature kRecordWebAudioEngagement;
MEDIA_EXPORT extern const base::Feature kResumeBackgroundVideo;
MEDIA_EXPORT extern const base::Feature kSpecCompliantCanPlayThrough;
MEDIA_EXPORT extern const base::Feature kUnifiedAutoplay;
MEDIA_EXPORT extern const base::Feature kUseAndroidOverlay;
MEDIA_EXPORT extern const base::Feature kUseAndroidOverlayAggressively;
MEDIA_EXPORT extern const base::Feature kUseFakeDeviceForMediaStream;
MEDIA_EXPORT extern const base::Feature kUseNewMediaCache;
MEDIA_EXPORT extern const base::Feature kUseR16Texture;
MEDIA_EXPORT extern const base::Feature kUseSurfaceLayerForVideo;
MEDIA_EXPORT extern const base::Feature kVaapiH264AMDEncoder;
MEDIA_EXPORT extern const base::Feature kVaapiLowPowerEncoder;
MEDIA_EXPORT extern const base::Feature kVaapiVP8Encoder;
MEDIA_EXPORT extern const base::Feature kVaapiVP9Encoder;
MEDIA_EXPORT extern const base::Feature kVideoBlitColorAccuracy;
MEDIA_EXPORT extern const base::Feature kWidevineAv1;
MEDIA_EXPORT extern const base::Feature kWidevineAv1ForceSupportForTesting;

#if defined(ARCH_CPU_X86_FAMILY) && defined(OS_CHROMEOS)
MEDIA_EXPORT extern const base::Feature kVp9kSVCHWDecoding;
#endif  // defined(ARCH_CPU_X86_FAMILY) && defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
MEDIA_EXPORT extern const base::Feature kMediaControlsExpandGesture;
MEDIA_EXPORT extern const base::Feature kMediaDrmPersistentLicense;
MEDIA_EXPORT extern const base::Feature kMediaDrmPreprovisioning;
MEDIA_EXPORT extern const base::Feature kMediaDrmPreprovisioningAtStartup;
MEDIA_EXPORT extern const base::Feature kAImageReaderVideoOutput;
MEDIA_EXPORT extern const base::Feature kDisableSurfaceLayerForVideo;
MEDIA_EXPORT extern const base::Feature kCanPlayHls;
MEDIA_EXPORT extern const base::Feature kPictureInPictureAPI;
MEDIA_EXPORT extern const base::Feature kHlsPlayer;
MEDIA_EXPORT extern const base::Feature kUseAudioLatencyFromHAL;
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
MEDIA_EXPORT extern const base::Feature kDelayCopyNV12Textures;
MEDIA_EXPORT extern const base::Feature kMediaFoundationH264Encoding;
MEDIA_EXPORT extern const base::Feature kMediaFoundationVideoCapture;
MEDIA_EXPORT extern const base::Feature kDirectShowGetPhotoState;
#endif  // defined(OS_WIN)

// Based on a |command_line| and the current platform, returns the effective
// autoplay policy. In other words, it will take into account the default policy
// if none is specified via the command line and options passed for testing.
// Returns one of the possible autoplay policy switches from the
// switches::autoplay namespace.
MEDIA_EXPORT std::string GetEffectiveAutoplayPolicy(
    const base::CommandLine& command_line);

MEDIA_EXPORT bool IsVideoCaptureAcceleratedJpegDecodingEnabled();

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
