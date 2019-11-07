// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "components/system_media_controls/linux/buildflags/buildflags.h"

namespace switches {

// Allow users to specify a custom buffer size for debugging purpose.
const char kAudioBufferSize[] = "audio-buffer-size";

// Set a timeout (in milliseconds) for the audio service to quit if there are no
// client connections to it. If the value is negative the service never quits.
const char kAudioServiceQuitTimeoutMs[] = "audio-service-quit-timeout-ms";

// Command line flag name to set the autoplay policy.
const char kAutoplayPolicy[] = "autoplay-policy";

const char kDisableAudioOutput[] = "disable-audio-output";

// Causes the AudioManager to fail creating audio streams. Used when testing
// various failure cases.
const char kFailAudioStreamCreation[] = "fail-audio-stream-creation";

// Set number of threads to use for video decoding.
const char kVideoThreads[] = "video-threads";

// Suspend media pipeline on background tabs.
const char kEnableMediaSuspend[] = "enable-media-suspend";
const char kDisableMediaSuspend[] = "disable-media-suspend";

// Force to report VP9 as an unsupported MIME type.
const char kReportVp9AsAnUnsupportedMimeType[] =
    "report-vp9-as-an-unsupported-mime-type";

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
// The Alsa device to use when opening an audio input stream.
const char kAlsaInputDevice[] = "alsa-input-device";
// The Alsa device to use when opening an audio stream.
const char kAlsaOutputDevice[] = "alsa-output-device";
#endif

#if defined(OS_WIN)
// Use exclusive mode audio streaming for Windows Vista and higher.
// Leads to lower latencies for audio streams which uses the
// AudioParameters::AUDIO_PCM_LOW_LATENCY audio path.
// See http://msdn.microsoft.com/en-us/library/windows/desktop/dd370844.aspx
// for details.
const char kEnableExclusiveAudio[] = "enable-exclusive-audio";

// Use Windows WaveOut/In audio API even if Core Audio is supported.
const char kForceWaveAudio[] = "force-wave-audio";

// Instead of always using the hardware channel layout, check if a driver
// supports the source channel layout.  Avoids outputting empty channels and
// permits drivers to enable stereo to multichannel expansion.  Kept behind a
// flag since some drivers lie about supported layouts and hang when used.  See
// http://crbug.com/259165 for more details.
const char kTrySupportedChannelLayouts[] = "try-supported-channel-layouts";

// Number of buffers to use for WaveOut.
const char kWaveOutBuffers[] = "waveout-buffers";
#endif

#if defined(USE_CRAS)
// Use CRAS, the ChromeOS audio server.
const char kUseCras[] = "use-cras";
#endif

// For automated testing of protected content, this switch allows specific
// domains (e.g. example.com) to skip asking the user for permission to share
// the protected media identifier. In this context, domain does not include the
// port number. User's content settings will not be affected by enabling this
// switch.
// Reference: http://crbug.com/718608
// Example:
// --unsafely-allow-protected-media-identifier-for-domain=a.com,b.ca
const char kUnsafelyAllowProtectedMediaIdentifierForDomain[] =
    "unsafely-allow-protected-media-identifier-for-domain";

// Use fake device for Media Stream to replace actual camera and microphone.
const char kUseFakeDeviceForMediaStream[] = "use-fake-device-for-media-stream";

// Use an .y4m file to play as the webcam. See the comments in
// media/capture/video/file_video_capture_device.h for more details.
const char kUseFileForFakeVideoCapture[] = "use-file-for-fake-video-capture";

// Play a .wav file as the microphone. Note that for WebRTC calls we'll treat
// the bits as if they came from the microphone, which means you should disable
// audio processing (lest your audio file will play back distorted). The input
// file is converted to suit Chrome's audio buses if necessary, so most sane
// .wav files should work. You can pass either <path> to play the file looping
// or <path>%noloop to stop after playing the file to completion.
const char kUseFileForFakeAudioCapture[] = "use-file-for-fake-audio-capture";

// Use a fake device for accelerated decoding of MJPEG. This allows, for
// example, testing of the communication to the GPU service without requiring
// actual accelerator hardware to be present.
const char kUseFakeMjpegDecodeAccelerator[] =
    "use-fake-mjpeg-decode-accelerator";

// Disable hardware acceleration of mjpeg decode for captured frame, where
// available.
const char kDisableAcceleratedMjpegDecode[] =
    "disable-accelerated-mjpeg-decode";

// When running tests on a system without the required hardware or libraries,
// this flag will cause the tests to fail. Otherwise, they silently succeed.
const char kRequireAudioHardwareForTesting[] =
    "require-audio-hardware-for-testing";

// Mutes audio sent to the audio device so it is not audible during
// automated testing.
const char kMuteAudio[] = "mute-audio";

// Allows clients to override the threshold for when the media renderer will
// declare the underflow state for the video stream when audio is present.
// TODO(dalecurtis): Remove once experiments for http://crbug.com/470940 finish.
const char kVideoUnderflowThresholdMs[] = "video-underflow-threshold-ms";

// Disables the new rendering algorithm for webrtc, which is designed to improve
// the rendering smoothness.
const char kDisableRTCSmoothnessAlgorithm[] =
    "disable-rtc-smoothness-algorithm";

// Force media player using SurfaceView instead of SurfaceTexture on Android.
const char kForceVideoOverlays[] = "force-video-overlays";

// Allows explicitly specifying MSE audio/video buffer sizes as megabytes.
// Default values are 150M for video and 12M for audio.
const char kMSEAudioBufferSizeLimitMb[] = "mse-audio-buffer-size-limit-mb";
const char kMSEVideoBufferSizeLimitMb[] = "mse-video-buffer-size-limit-mb";

// Specifies the path to the Clear Key CDM for testing, which is necessary to
// support External Clear Key key system when library CDM is enabled. Note that
// External Clear Key key system support is also controlled by feature
// kExternalClearKeyForTesting.
const char kClearKeyCdmPathForTesting[] = "clear-key-cdm-path-for-testing";

// Overrides the default enabled library CDM interface version(s) with the one
// specified with this switch, which will be the only version enabled. For
// example, on a build where CDM 8, CDM 9 and CDM 10 are all supported
// (implemented), but only CDM 8 and CDM 9 are enabled by default:
//  --override-enabled-cdm-interface-version=8 : Only CDM 8 is enabled
//  --override-enabled-cdm-interface-version=9 : Only CDM 9 is enabled
//  --override-enabled-cdm-interface-version=10 : Only CDM 10 is enabled
//  --override-enabled-cdm-interface-version=11 : No CDM interface is enabled
// This can be used for local testing and debugging. It can also be used to
// enable an experimental CDM interface (which is always disabled by default)
// for testing while it's still in development.
const char kOverrideEnabledCdmInterfaceVersion[] =
    "override-enabled-cdm-interface-version";

// Overrides hardware secure codecs support for testing. If specified, real
// platform hardware secure codecs check will be skipped. Codecs are separated
// by comma. Valid codecs are "vp8", "vp9" and "avc1". For example:
//  --override-hardware-secure-codecs-for-testing=vp8,vp9
//  --override-hardware-secure-codecs-for-testing=avc1
// CENC encryption scheme is assumed to be supported for the specified codecs.
// If no valid codecs specified, no hardware secure codecs are supported. This
// can be used to disable hardware secure codecs support:
//  --override-hardware-secure-codecs-for-testing
const char kOverrideHardwareSecureCodecsForTesting[] =
    "override-hardware-secure-codecs-for-testing";

// Enables GpuMemoryBuffer-based buffer pool.
const char kVideoCaptureUseGpuMemoryBuffer[] =
    "video-capture-use-gpu-memory-buffer";

namespace autoplay {

// Autoplay policy that requires a document user activation.
const char kDocumentUserActivationRequiredPolicy[] =
    "document-user-activation-required";

// Autoplay policy that does not require any user gesture.
const char kNoUserGestureRequiredPolicy[] = "no-user-gesture-required";

// Autoplay policy to require a user gesture in order to play.
const char kUserGestureRequiredPolicy[] = "user-gesture-required";

}  // namespace autoplay

}  // namespace switches

namespace media {

// Prefer FFmpeg to LibVPX for Vp8 decoding with opaque alpha mode.
const base::Feature kFFmpegDecodeOpaqueVP8{"FFmpegDecodeOpaqueVP8",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Only used for disabling overlay fullscreen (aka SurfaceView) in Clank.
const base::Feature kOverlayFullscreenVideo{"overlay-fullscreen-video",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enable Picture-in-Picture.
const base::Feature kPictureInPicture {
  "PictureInPicture",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Only decode preload=metadata elements upon visibility.
// TODO(crbug.com/879406): Remove this after M76 ships to stable
const base::Feature kPreloadMetadataLazyLoad{"PreloadMetadataLazyLoad",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Let videos be resumed via remote controls (for example, the notification)
// when in background.
const base::Feature kResumeBackgroundVideo {
  "resume-background-video",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable Media Capabilities with finch-parameters.
const base::Feature kMediaCapabilitiesWithParameters{
    "MediaCapabilitiesWithParameters", base::FEATURE_ENABLED_BY_DEFAULT};

// Display the Cast overlay button on the media controls.
const base::Feature kMediaCastOverlayButton{"MediaCastOverlayButton",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Use AndroidOverlay for more cases than just player-element fullscreen?  This
// requires that |kOverlayFullscreenVideo| is true, else it is ignored.
const base::Feature kUseAndroidOverlayAggressively{
    "UseAndroidOverlayAggressively", base::FEATURE_ENABLED_BY_DEFAULT};

// Let video without audio be paused when it is playing in the background.
const base::Feature kBackgroundVideoPauseOptimization{
    "BackgroundVideoPauseOptimization", base::FEATURE_ENABLED_BY_DEFAULT};

// Make MSE garbage collection algorithm more aggressive when we are under
// moderate or critical memory pressure. This will relieve memory pressure by
// releasing stale data from MSE buffers.
const base::Feature kMemoryPressureBasedSourceBufferGC{
    "MemoryPressureBasedSourceBufferGC", base::FEATURE_DISABLED_BY_DEFAULT};

// Approach original pre-REC MSE object URL autorevoking behavior, though await
// actual attempt to use the object URL for attachment to perform revocation.
// This will hopefully reduce runtime memory bloat for pages that do not
// explicitly detach their HTMLME+MSE object collections nor explicitly revoke
// the object URLs used to attach HTMLME+MSE. When disabled, revocation only
// occurs when application explicitly revokes the object URL, or upon the
// execution context teardown for the MediaSource object. When enabled,
// revocation occurs upon successful start of attachment of HTMLME to the object
// URL. Note, rather than immediately scheduling a task to revoke upon the URL's
// creation, as at least one other browser does and the original File API
// pattern used to follow, this delay until attachment start enables new
// scenarios that could use the object URL for attaching HTMLME+MSE cross-thread
// (MSE-in-workers), where there could be significant delay between the worker
// thread creation of the object URL and the main thread usage of the object URL
// for starting attachment to HTMLME.
const base::Feature kRevokeMediaSourceObjectURLOnAttach{
    "RevokeMediaSourceObjectURLOnAttach", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the instance from ChromeosVideoDecoderFactory in
// MojoVideoDecoderService, replacing VdaVideoDecoder at Chrome OS platform.
const base::Feature kChromeosVideoDecoder{"ChromeosVideoDecoder",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Don't allow use of 11.1 devices, even if supported. They might be more crashy
const base::Feature kD3D11LimitTo11_0{"D3D11VideoDecoderLimitTo11_0",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable saving playback information in a crash trace, to see if some codecs
// are crashier than others.
const base::Feature kD3D11PrintCodecOnCrash{"D3D11PrintCodecOnCrash",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enable The D3D11 Video decoder.
const base::Feature kD3D11VideoDecoder{"D3D11VideoDecoder",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Tell D3D11VideoDecoder to ignore workarounds for zero copy.  Requires that
// kD3D11VideoDecoder is enabled.
const base::Feature kD3D11VideoDecoderIgnoreWorkarounds{
    "D3D11VideoDecoderIgnoreWorkarounds", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable D3D11VideoDecoder to decode VP9 profile 2 (10 bit) video.
const base::Feature kD3D11VideoDecoderVP9Profile2{
    "D3D11VideoDecoderEnableVP9Profile2", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable D3D11VideoDecoder to copy pictures based on workarounds, rather
// than binding them.
const base::Feature kD3D11VideoDecoderCopyPictures{
    "D3D11VideoDecoderCopyPictures", base::FEATURE_DISABLED_BY_DEFAULT};

// Falls back to other decoders after audio/video decode error happens. The
// implementation may choose different strategies on when to fallback. See
// DecoderStream for details. When disabled, playback will fail immediately
// after a decode error happens. This can be useful in debugging and testing
// because the behavior is simpler and more predictable.
const base::Feature kFallbackAfterDecodeError{"FallbackAfterDecodeError",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Show toolbar button that opens dialog for controlling media sessions.
const base::Feature kGlobalMediaControls{"GlobalMediaControls",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Show Cast sessions in Global Media Controls. It is no-op if
// kGlobalMediaControls is not enabled.
const base::Feature kGlobalMediaControlsForCast{
    "GlobalMediaControlsForCast", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable new cpu load estimator. Intended for evaluation in local
// testing and origin-trial.
// TODO(nisse): Delete once we have switched over to always using the
// new estimator.
const base::Feature kNewEncodeCpuLoadEstimator{
    "NewEncodeCpuLoadEstimator", base::FEATURE_DISABLED_BY_DEFAULT};

// CanPlayThrough issued according to standard.
const base::Feature kSpecCompliantCanPlayThrough{
    "SpecCompliantCanPlayThrough", base::FEATURE_ENABLED_BY_DEFAULT};

// Use shared block-based buffering for media.
const base::Feature kUseNewMediaCache{"use-new-media-cache",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using the media history store to store media engagement metrics.
const base::Feature kUseMediaHistoryStore{"UseMediaHistoryStore",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Use R16 texture for 9-16 bit channel instead of half-float conversion by CPU.
const base::Feature kUseR16Texture{"use-r16-texture",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Unified Autoplay policy by overriding the platform's default
// autoplay policy.
const base::Feature kUnifiedAutoplay{"UnifiedAutoplay",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, use SurfaceLayer instead of VideoLayer for all playbacks that
// aren't MediaStream.
const base::Feature kUseSurfaceLayerForVideo{"UseSurfaceLayerForVideo",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enable VA-API hardware encode acceleration for H264 on AMD.
const base::Feature kVaapiH264AMDEncoder{"VaapiH264AMDEncoder",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enable VA-API hardware low power encoder for all codecs.
const base::Feature kVaapiLowPowerEncoder{"VaapiLowPowerEncoder",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enable VA-API hardware encode acceleration for VP8.
const base::Feature kVaapiVP8Encoder{"VaapiVP8Encoder",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable VA-API hardware encode acceleration for VP9.
const base::Feature kVaapiVP9Encoder{"VaapiVP9Encoder",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(ARCH_CPU_X86_FAMILY) && defined(OS_CHROMEOS)
// Enable VP9 k-SVC decoding with HW decoder for webrtc use case on ChromeOS.
const base::Feature kVp9kSVCHWDecoding{"Vp9kSVCHWDecoding",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif  //  defined(ARCH_CPU_X86_FAMILY) && defined(OS_CHROMEOS)

// Inform video blitter of video color space.
const base::Feature kVideoBlitColorAccuracy{"video-blit-color-accuracy",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables support for External Clear Key (ECK) key system for testing on
// supported platforms. On platforms that do not support ECK, this feature has
// no effect.
const base::Feature kExternalClearKeyForTesting{
    "ExternalClearKeyForTesting", base::FEATURE_DISABLED_BY_DEFAULT};

// Prevents UrlProvisionFetcher from making a provisioning request. If
// specified, any provisioning request made will not be sent to the provisioning
// server, and the response will indicate a failure to communicate with the
// provisioning server.
const base::Feature kFailUrlProvisionFetcherForTesting{
    "FailUrlProvisionFetcherForTesting", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables hardware secure decryption if supported by hardware and CDM.
// TODO(xhwang): Currently this is only used for development of new features.
// Apply this to Android and ChromeOS as well where hardware secure decryption
// is already available.
const base::Feature kHardwareSecureDecryption{
    "HardwareSecureDecryption", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables encrypted AV1 support in EME requestMediaKeySystemAccess() query by
// Widevine key system if it is also supported by the underlying Widevine CDM.
// This feature does not affect the actual playback of encrypted AV1 if it's
// served by the player regardless of the query result.
const base::Feature kWidevineAv1{"WidevineAv1",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// Forces to support encrypted AV1 in EME requestMediaKeySystemAccess() query by
// Widevine key system even if the underlying Widevine CDM doesn't support it.
// No effect if "WidevineAv1" feature is disabled.
const base::Feature kWidevineAv1ForceSupportForTesting{
    "WidevineAv1ForceSupportForTesting", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables handling of hardware media keys for controlling media.
const base::Feature kHardwareMediaKeyHandling{
  "HardwareMediaKeyHandling",
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MACOSX) || \
    BUILDFLAG(USE_MPRIS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enables low-delay video rendering in media pipeline on "live" stream.
const base::Feature kLowDelayVideoRenderingOnLiveStream{
    "low-delay-video-rendering-on-live-stream",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Whether the autoplay policy should ignore Web Audio. When ignored, the
// autoplay policy will be hardcoded to be the legacy one on based on the
// platform
const base::Feature kAutoplayIgnoreWebAudio{"AutoplayIgnoreWebAudio",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Whether we should show a setting to disable autoplay policy.
const base::Feature kAutoplayDisableSettings{"AutoplayDisableSettings",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Whether we should allow autoplay whitelisting via sounds settings.
const base::Feature kAutoplayWhitelistSettings{
    "AutoplayWhitelistSettings", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Enable a gesture to make the media controls expaned into the display cutout.
// TODO(beccahughes): Remove this.
const base::Feature kMediaControlsExpandGesture{
    "MediaControlsExpandGesture", base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental feature to enable persistent-license type support in MediaDrm
// when using Encrypted Media Extensions (EME) API.
// TODO(xhwang): Remove this after feature launch. See http://crbug.com/493521
const base::Feature kMediaDrmPersistentLicense{
    "MediaDrmPersistentLicense", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables MediaDrmOriginIdManager to provide preprovisioned origin IDs for
// MediaDrmBridge. If disabled, MediaDrmBridge will get unprovisioned origin IDs
// which will trigger provisioning process after MediaDrmBridge is created.
const base::Feature kMediaDrmPreprovisioning{"MediaDrmPreprovisioning",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Determines if MediaDrmOriginIdManager should attempt to pre-provision origin
// IDs at startup (whenever a profile is loaded). Also used by tests that
// disable it so that the tests can setup before pre-provisioning is done.
// Note: Has no effect if kMediaDrmPreprovisioning feature is disabled.
const base::Feature kMediaDrmPreprovisioningAtStartup{
    "MediaDrmPreprovisioningAtStartup", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Android Image Reader path for Video decoding(for AVDA and MCVD)
const base::Feature kAImageReaderVideoOutput{"AImageReaderVideoOutput",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Prevents using SurfaceLayer for videos. This is meant to be used by embedders
// that cannot support SurfaceLayer at the moment.
const base::Feature kDisableSurfaceLayerForVideo{
    "DisableSurfaceLayerForVideo", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable picture in picture web api for android.
const base::Feature kPictureInPictureAPI{"PictureInPictureAPI",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables CanPlayType() (and other queries) for HLS MIME types. Note that
// disabling this also causes navigation to .m3u8 files to trigger downloading
// instead of playback.
const base::Feature kCanPlayHls{"CanPlayHls", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of MediaPlayerRenderer for HLS playback. When disabled,
// HLS manifests will fail to load (triggering source fallback or load error).
const base::Feature kHlsPlayer{"HlsPlayer", base::FEATURE_ENABLED_BY_DEFAULT};

// Use the (hacky) AudioManager.getOutputLatency() call to get the estimated
// hardware latency for a stream for OpenSLES playback.  This is normally not
// needed, except for some Android TV devices.
const base::Feature kUseAudioLatencyFromHAL{"UseAudioLatencyFromHAL",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enable pooling of SharedImageVideo objects for use by MCVD, to save a hop to
// the GPU main thread during VideoFrame construction.
const base::Feature kUsePooledSharedImageVideoProvider{
    "UsePooledSharedImageVideoProvider", base::FEATURE_ENABLED_BY_DEFAULT};

#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
// Does NV12->NV12 video copy on the main thread right before the texture's
// used by GL.
const base::Feature kDelayCopyNV12Textures{"DelayCopyNV12Textures",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables H264 HW encode acceleration using Media Foundation for Windows.
const base::Feature kMediaFoundationH264Encoding{
    "MediaFoundationH264Encoding", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables MediaFoundation based video capture
const base::Feature kMediaFoundationVideoCapture{
    "MediaFoundationVideoCapture", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables DirectShow GetPhotoState implementation
// Created to act as a kill switch by disabling it, in the case of the
// resurgence of https://crbug.com/722038
const base::Feature kDirectShowGetPhotoState{"DirectShowGetPhotoState",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

#endif  // defined(OS_WIN)

std::string GetEffectiveAutoplayPolicy(const base::CommandLine& command_line) {
  // Return the autoplay policy set in the command line, if any.
  if (command_line.HasSwitch(switches::kAutoplayPolicy))
    return command_line.GetSwitchValueASCII(switches::kAutoplayPolicy);

  if (base::FeatureList::IsEnabled(media::kUnifiedAutoplay))
    return switches::autoplay::kDocumentUserActivationRequiredPolicy;

// The default value is platform dependent.
#if defined(OS_ANDROID)
  return switches::autoplay::kUserGestureRequiredPolicy;
#else
  return switches::autoplay::kNoUserGestureRequiredPolicy;
#endif
}

// Adds icons to the overflow menu on the native media controls.
// TODO(steimel): Remove this.
const base::Feature kOverflowIconsForMediaControls{
    "OverflowIconsForMediaControls", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Media Engagement Index recording. This data will be used to determine
// when to bypass autoplay policies. This is recorded on all platforms.
const base::Feature kRecordMediaEngagementScores{
    "RecordMediaEngagementScores", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Media Engagement Index recording for Web Audio playbacks.
const base::Feature kRecordWebAudioEngagement{"RecordWebAudioEngagement",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// The following Media Engagement flags are not enabled on mobile platforms:
// - MediaEngagementBypassAutoplayPolicies: enables the Media Engagement Index
//   data to be esude to override autoplay policies. An origin with a high MEI
//   will be allowed to autoplay.
// - PreloadMediaEngagementData: enables a list of origins to be considered as
//   having a high MEI until there is enough local data to determine the user's
//   preferred behaviour.
#if defined(OS_ANDROID) || defined(OS_IOS)
const base::Feature kMediaEngagementBypassAutoplayPolicies{
    "MediaEngagementBypassAutoplayPolicies", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPreloadMediaEngagementData{
    "PreloadMediaEngagementData", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kMediaEngagementBypassAutoplayPolicies{
    "MediaEngagementBypassAutoplayPolicies", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kPreloadMediaEngagementData{
    "PreloadMediaEngagementData", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

const base::Feature kMediaEngagementHTTPSOnly{
    "MediaEngagementHTTPSOnly", base::FEATURE_DISABLED_BY_DEFAULT};

// Send events to devtools rather than to chrome://media-internals
const base::Feature kMediaInspectorLogging{"MediaInspectorLogging",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables experimental local learning for media. Used in the context of media
// capabilities only. Adds reporting only; does not change media behavior.
const base::Feature kMediaLearningExperiment{"MediaLearningExperiment",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the general purpose media machine learning framework. Adds reporting
// only; does not change media behavior.
const base::Feature kMediaLearningFramework{"MediaLearningFramework",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enable aggregate power measurement for media playback.
const base::Feature kMediaPowerExperiment{"MediaPowerExperiment",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables flash to be ducked by audio focus. This is enabled on Chrome OS which
// has audio focus enabled.
const base::Feature kAudioFocusDuckFlash {
  "AudioFocusDuckFlash",
#if defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Only affects Android. Suspends a media session when audio focus is lost; when
// this setting is disabled, an Android media session will not be suspended when
// Audio focus is lost. This is used by Cast which sometimes needs to drive
// multiple media sessions.
const base::Feature kAudioFocusLossSuspendMediaSession{
    "AudioFocusMediaSession", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the internal Media Session logic without enabling the Media Session
// service.
const base::Feature kInternalMediaSession {
  "InternalMediaSession",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kUseFakeDeviceForMediaStream{
    "use-fake-device-for-media-stream", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsVideoCaptureAcceleratedJpegDecodingEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAcceleratedMjpegDecode)) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeMjpegDecodeAccelerator)) {
    return true;
  }
#if defined(OS_CHROMEOS)
  return true;
#endif
  return false;
}

}  // namespace media
