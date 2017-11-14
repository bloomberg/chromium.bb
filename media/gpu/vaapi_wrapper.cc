// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi_wrapper.h"

#include <dlfcn.h>
#include <string.h>

#include <va/va.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "build/build_config.h"

// Auto-generated for dlopen libva libraries
#include "media/gpu/vaapi/va_stubs.h"

#include "media/gpu/vaapi/vaapi_picture.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gl/gl_bindings.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"  // nogncheck
#elif defined(USE_OZONE)
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_version.h>
#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif  // USE_X11

using media_gpu_vaapi::kModuleVa;
#if defined(USE_X11)
using media_gpu_vaapi::kModuleVa_x11;
#elif defined(USE_OZONE)
using media_gpu_vaapi::kModuleVa_drm;
#endif  // USE_X11
using media_gpu_vaapi::InitializeStubs;
using media_gpu_vaapi::StubPathMap;

#define LOG_VA_ERROR_AND_REPORT(va_error, err_msg)                  \
  do {                                                              \
    LOG(ERROR) << err_msg << " VA error: " << vaErrorStr(va_error); \
    report_error_to_uma_cb_.Run();                                  \
  } while (0)

#define VA_LOG_ON_ERROR(va_error, err_msg)        \
  do {                                            \
    if ((va_error) != VA_STATUS_SUCCESS)          \
      LOG_VA_ERROR_AND_REPORT(va_error, err_msg); \
  } while (0)

#define VA_SUCCESS_OR_RETURN(va_error, err_msg, ret) \
  do {                                               \
    if ((va_error) != VA_STATUS_SUCCESS) {           \
      LOG_VA_ERROR_AND_REPORT(va_error, err_msg);    \
      return (ret);                                  \
    }                                                \
  } while (0)

#if defined(USE_OZONE)
namespace {

uint32_t BufferFormatToVAFourCC(gfx::BufferFormat fmt) {
  switch (fmt) {
    case gfx::BufferFormat::BGRX_8888:
      return VA_FOURCC_BGRX;
    case gfx::BufferFormat::BGRA_8888:
      return VA_FOURCC_BGRA;
    case gfx::BufferFormat::UYVY_422:
      return VA_FOURCC_UYVY;
    case gfx::BufferFormat::YVU_420:
      return VA_FOURCC_YV12;
    default:
      NOTREACHED();
      return 0;
  }
}

uint32_t BufferFormatToVARTFormat(gfx::BufferFormat fmt) {
  switch (fmt) {
    case gfx::BufferFormat::UYVY_422:
      return VA_RT_FORMAT_YUV422;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_8888:
      return VA_RT_FORMAT_RGB32;
    case gfx::BufferFormat::YVU_420:
      return VA_RT_FORMAT_YUV420;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace
#endif

namespace media {

namespace {

// Maximum framerate of encoded profile. This value is an arbitary limit
// and not taken from HW documentation.
const int kMaxEncoderFramerate = 30;

// Config attributes common for both encode and decode.
static const VAConfigAttrib kCommonVAConfigAttribs[] = {
    {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
};

// Attributes required for encode.
static const VAConfigAttrib kEncodeVAConfigAttribs[] = {
    {VAConfigAttribRateControl, VA_RC_CBR},
    {VAConfigAttribEncPackedHeaders,
     VA_ENC_PACKED_HEADER_SEQUENCE | VA_ENC_PACKED_HEADER_PICTURE},
};

// A map between VideoCodecProfile and VAProfile.
static const struct {
  VideoCodecProfile profile;
  VAProfile va_profile;
} kProfileMap[] = {
    {H264PROFILE_BASELINE, VAProfileH264Baseline},
    {H264PROFILE_MAIN, VAProfileH264Main},
    // TODO(posciak): See if we can/want to support other variants of
    // H264PROFILE_HIGH*.
    {H264PROFILE_HIGH, VAProfileH264High},
    {VP8PROFILE_ANY, VAProfileVP8Version0_3},
    {VP9PROFILE_PROFILE0, VAProfileVP9Profile0},
    {VP9PROFILE_PROFILE1, VAProfileVP9Profile1},
    // TODO(mcasas): support other VP9 Profiles, https://crbug.com/778093.
};

// This class is a wrapper around its |va_display_| (and its associated
// |va_lock_|) to guarantee mutual exclusion and singleton behaviour.
class VADisplayState {
 public:
  static VADisplayState* Get();

  // Initialize static data before sandbox is enabled.
  static void PreSandboxInitialization();
  // Returns false on init failure.
  static bool PostSandboxInitialization();

  VADisplayState();
  ~VADisplayState() = delete;

  // |va_lock_| must be held on entry.
  bool Initialize();
  void Deinitialize(VAStatus* status);

  base::Lock* va_lock() { return &va_lock_; }
  VADisplay va_display() const { return va_display_; }

#if defined(USE_OZONE)
  void SetDrmFd(base::PlatformFile fd) { drm_fd_.reset(HANDLE_EINTR(dup(fd))); }
#endif  // USE_OZONE

 private:
  // Protected by |va_lock_|.
  int refcount_;

  // Libva is not thread safe, so we have to do locking for it ourselves.
  // This lock is to be taken for the duration of all VA-API calls and for
  // the entire job submission sequence in ExecuteAndDestroyPendingBuffers().
  base::Lock va_lock_;

#if defined(USE_OZONE)
  // Drm fd used to obtain access to the driver interface by VA.
  base::ScopedFD drm_fd_;
#endif  // USE_OZONE

  // The VADisplay handle.
  VADisplay va_display_;

  // True if vaInitialize() has been called successfully.
  bool va_initialized_;
};

// static
VADisplayState* VADisplayState::Get() {
  static VADisplayState* display_state = new VADisplayState();
  return display_state;
}

// static
void VADisplayState::PreSandboxInitialization() {
#if defined(USE_OZONE)
  const char kDriRenderNode0Path[] = "/dev/dri/renderD128";
  base::File drm_file = base::File(
      base::FilePath::FromUTF8Unsafe(kDriRenderNode0Path),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  if (drm_file.IsValid())
    VADisplayState::Get()->SetDrmFd(drm_file.GetPlatformFile());
#endif
}

// static
bool VADisplayState::PostSandboxInitialization() {
  StubPathMap paths;
  paths[kModuleVa].push_back("libva.so.1");
#if defined(USE_X11)
  paths[kModuleVa_x11].push_back("libva-x11.so.1");
#elif defined(USE_OZONE)
  paths[kModuleVa_drm].push_back("libva-drm.so.1");
#endif

  const bool success = InitializeStubs(paths);
  if (!success) {
    static const char kErrorMsg[] = "Failed to initialize VAAPI libs";
#if defined(OS_CHROMEOS)
    // When Chrome runs on Linux with target_os="chromeos", do not log error
    // message without VAAPI libraries.
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS()) << kErrorMsg;
#else
    DVLOG(1) << kErrorMsg;
#endif
  }
  return success;
}

VADisplayState::VADisplayState()
    : refcount_(0), va_display_(nullptr), va_initialized_(false) {}

bool VADisplayState::Initialize() {
  va_lock_.AssertAcquired();
  if (refcount_++ > 0)
    return true;
#if defined(USE_X11)
  va_display_ = vaGetDisplay(gfx::GetXDisplay());
#elif defined(USE_OZONE)
  va_display_ = vaGetDisplayDRM(drm_fd_.get());
#endif  // USE_X11

  if (!vaDisplayIsValid(va_display_)) {
    LOG(ERROR) << "Could not get a valid VA display";
    return false;
  }

  // Set VA logging level to enable error messages, unless already set
  constexpr char libva_log_level_env[] = "LIBVA_MESSAGING_LEVEL";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!env->HasVar(libva_log_level_env))
    env->SetVar(libva_log_level_env, "1");

  // The VAAPI version.
  int major_version, minor_version;
  VAStatus va_res = vaInitialize(va_display_, &major_version, &minor_version);
  if (va_res != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaInitialize failed: " << vaErrorStr(va_res);
    return false;
  }

  va_initialized_ = true;
  DVLOG(1) << "VAAPI version: " << major_version << "." << minor_version;

  if (major_version != VA_MAJOR_VERSION || minor_version != VA_MINOR_VERSION) {
    LOG(ERROR) << "This build of Chromium requires VA-API version "
               << VA_MAJOR_VERSION << "." << VA_MINOR_VERSION
               << ", system version: " << major_version << "." << minor_version;
    return false;
  }
  return true;
}

void VADisplayState::Deinitialize(VAStatus* status) {
  va_lock_.AssertAcquired();
  if (--refcount_ > 0)
    return;

  // Must check if vaInitialize completed successfully, to work around a bug in
  // libva. The bug was fixed upstream:
  // http://lists.freedesktop.org/archives/libva/2013-July/001807.html
  // TODO(mgiuca): Remove this check, and the |va_initialized_| variable, once
  // the fix has rolled out sufficiently.
  if (va_initialized_ && va_display_)
    *status = vaTerminate(va_display_);
  va_initialized_ = false;
  va_display_ = nullptr;
}

static std::vector<VAConfigAttrib> GetRequiredAttribs(
    VaapiWrapper::CodecMode mode) {
  std::vector<VAConfigAttrib> required_attribs;
  required_attribs.insert(
      required_attribs.end(), kCommonVAConfigAttribs,
      kCommonVAConfigAttribs + arraysize(kCommonVAConfigAttribs));
  if (mode == VaapiWrapper::kEncode) {
    required_attribs.insert(
        required_attribs.end(), kEncodeVAConfigAttribs,
        kEncodeVAConfigAttribs + arraysize(kEncodeVAConfigAttribs));
  }
  return required_attribs;
}

// This class encapsulates reading and giving access to the list of supported
// ProfileInfo entries, in a singleton way.
class LazyProfileInfos {
 public:
  struct ProfileInfo {
    VAProfile va_profile;
    gfx::Size max_resolution;
  };
  static LazyProfileInfos* Get();

  std::vector<ProfileInfo> GetSupportedProfileInfosForCodecMode(
      VaapiWrapper::CodecMode mode);

  bool IsProfileSupported(VaapiWrapper::CodecMode mode, VAProfile va_profile);

 private:
  LazyProfileInfos();
  ~LazyProfileInfos();

  bool GetSupportedVAProfiles(std::vector<VAProfile>* profiles);

  // Gets supported profile infos for |mode|.
  std::vector<ProfileInfo> GetSupportedProfileInfosForCodecModeInternal(
      VaapiWrapper::CodecMode mode);

  // |va_lock_| must be held on entry in the following _Locked methods.

  // Checks if |va_profile| supports |entrypoint| or not.
  bool IsEntrypointSupported_Locked(VAProfile va_profile,
                                    VAEntrypoint entrypoint);
  // Returns true if |va_profile| for |entrypoint| with |required_attribs| is
  // supported.
  bool AreAttribsSupported_Locked(
      VAProfile va_profile,
      VAEntrypoint entrypoint,
      const std::vector<VAConfigAttrib>& required_attribs);
  // Gets maximum resolution for |va_profile| and |entrypoint| with
  // |required_attribs|. If return value is true, |resolution| is the maximum
  // resolution.
  bool GetMaxResolution_Locked(VAProfile va_profile,
                               VAEntrypoint entrypoint,
                               std::vector<VAConfigAttrib>& required_attribs,
                               gfx::Size* resolution);

  std::vector<ProfileInfo> supported_profiles_[VaapiWrapper::kCodecModeMax];

  // Pointer to VADisplayState's members |va_lock_| and its |va_display_|.
  base::Lock* va_lock_;
  VADisplay va_display_;

  const base::Closure report_error_to_uma_cb_;
};

}  // namespace

VaapiWrapper::VaapiWrapper()
    : va_surface_format_(0),
      va_display_(NULL),
      va_config_id_(VA_INVALID_ID),
      va_context_id_(VA_INVALID_ID),
      va_vpp_config_id_(VA_INVALID_ID),
      va_vpp_context_id_(VA_INVALID_ID),
      va_vpp_buffer_id_(VA_INVALID_ID) {
  va_lock_ = VADisplayState::Get()->va_lock();
}

VaapiWrapper::~VaapiWrapper() {
  DestroyPendingBuffers();
  DestroyCodedBuffers();
  DestroySurfaces();
  DeinitializeVpp();
  Deinitialize();
}

// static
scoped_refptr<VaapiWrapper> VaapiWrapper::Create(
    CodecMode mode,
    VAProfile va_profile,
    const base::Closure& report_error_to_uma_cb) {
  if (!LazyProfileInfos::Get()->IsProfileSupported(mode, va_profile)) {
    DVLOG(1) << "Unsupported va_profile: " << va_profile;
    return nullptr;
  }

  scoped_refptr<VaapiWrapper> vaapi_wrapper(new VaapiWrapper());
  if (vaapi_wrapper->VaInitialize(report_error_to_uma_cb)) {
    if (vaapi_wrapper->Initialize(mode, va_profile))
      return vaapi_wrapper;
  }
  LOG(ERROR) << "Failed to create VaapiWrapper for va_profile: " << va_profile;
  return nullptr;
}

// static
scoped_refptr<VaapiWrapper> VaapiWrapper::CreateForVideoCodec(
    CodecMode mode,
    VideoCodecProfile profile,
    const base::Closure& report_error_to_uma_cb) {
  VAProfile va_profile = ProfileToVAProfile(profile, mode);
  return Create(mode, va_profile, report_error_to_uma_cb);
}

// static
VideoEncodeAccelerator::SupportedProfiles
VaapiWrapper::GetSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles profiles;
  std::vector<LazyProfileInfos::ProfileInfo> encode_profile_infos =
      LazyProfileInfos::Get()->GetSupportedProfileInfosForCodecMode(kEncode);

  for (size_t i = 0; i < arraysize(kProfileMap); ++i) {
    VAProfile va_profile = ProfileToVAProfile(kProfileMap[i].profile, kEncode);
    if (va_profile == VAProfileNone)
      continue;
    for (const auto& profile_info : encode_profile_infos) {
      if (profile_info.va_profile == va_profile) {
        VideoEncodeAccelerator::SupportedProfile profile;
        profile.profile = kProfileMap[i].profile;
        profile.max_resolution = profile_info.max_resolution;
        profile.max_framerate_numerator = kMaxEncoderFramerate;
        profile.max_framerate_denominator = 1;
        profiles.push_back(profile);
        break;
      }
    }
  }
  return profiles;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VaapiWrapper::GetSupportedDecodeProfiles() {
  VideoDecodeAccelerator::SupportedProfiles profiles;
  std::vector<LazyProfileInfos::ProfileInfo> decode_profile_infos =
      LazyProfileInfos::Get()->GetSupportedProfileInfosForCodecMode(kDecode);

  for (size_t i = 0; i < arraysize(kProfileMap); ++i) {
    VAProfile va_profile = ProfileToVAProfile(kProfileMap[i].profile, kDecode);
    if (va_profile == VAProfileNone)
      continue;
    for (const auto& profile_info : decode_profile_infos) {
      if (profile_info.va_profile == va_profile) {
        VideoDecodeAccelerator::SupportedProfile profile;
        profile.profile = kProfileMap[i].profile;
        profile.max_resolution = profile_info.max_resolution;
        profile.min_resolution.SetSize(16, 16);
        profiles.push_back(profile);
        break;
      }
    }
  }
  return profiles;
}

// static
bool VaapiWrapper::IsJpegDecodeSupported() {
  return LazyProfileInfos::Get()->IsProfileSupported(kDecode,
                                                     VAProfileJPEGBaseline);
}

void VaapiWrapper::TryToSetVADisplayAttributeToLocalGPU() {
  base::AutoLock auto_lock(*va_lock_);
  VADisplayAttribute item = {VADisplayAttribRenderMode,
                             1,   // At least support '_LOCAL_OVERLAY'.
                             -1,  // The maximum possible support 'ALL'.
                             VA_RENDER_MODE_LOCAL_GPU,
                             VA_DISPLAY_ATTRIB_SETTABLE};

  VAStatus va_res = vaSetDisplayAttributes(va_display_, &item, 1);
  if (va_res != VA_STATUS_SUCCESS)
    DVLOG(2) << "vaSetDisplayAttributes unsupported, ignoring by default.";
}

// static
VAProfile VaapiWrapper::ProfileToVAProfile(VideoCodecProfile profile,
                                           CodecMode mode) {
  VAProfile va_profile = VAProfileNone;
  for (size_t i = 0; i < arraysize(kProfileMap); ++i) {
    if (kProfileMap[i].profile == profile) {
      va_profile = kProfileMap[i].va_profile;
      break;
    }
  }
  if (!LazyProfileInfos::Get()->IsProfileSupported(mode, va_profile) &&
      va_profile == VAProfileH264Baseline) {
    // https://crbug.com/345569: ProfileIDToVideoCodecProfile() currently strips
    // the information whether the profile is constrained or not, so we have no
    // way to know here. Try for baseline first, but if it is not supported,
    // try constrained baseline and hope this is what it actually is
    // (which in practice is true for a great majority of cases).
    if (LazyProfileInfos::Get()->IsProfileSupported(
            mode, VAProfileH264ConstrainedBaseline)) {
      va_profile = VAProfileH264ConstrainedBaseline;
      DVLOG(1) << "Fall back to constrained baseline profile.";
    }
  }
  return va_profile;
}

std::vector<LazyProfileInfos::ProfileInfo>
LazyProfileInfos::GetSupportedProfileInfosForCodecModeInternal(
    VaapiWrapper::CodecMode mode) {
  std::vector<ProfileInfo> supported_profile_infos;
  std::vector<VAProfile> va_profiles;
  if (!GetSupportedVAProfiles(&va_profiles))
    return supported_profile_infos;

  std::vector<VAConfigAttrib> required_attribs = GetRequiredAttribs(mode);
  VAEntrypoint entrypoint =
      (mode == VaapiWrapper::kEncode ? VAEntrypointEncSlice : VAEntrypointVLD);

  base::AutoLock auto_lock(*va_lock_);
  for (const auto& va_profile : va_profiles) {
    if (!IsEntrypointSupported_Locked(va_profile, entrypoint))
      continue;
    if (!AreAttribsSupported_Locked(va_profile, entrypoint, required_attribs))
      continue;
    ProfileInfo profile_info;
    if (!GetMaxResolution_Locked(va_profile, entrypoint, required_attribs,
                                 &profile_info.max_resolution)) {
      LOG(ERROR) << "GetMaxResolution failed for va_profile " << va_profile
                 << " and entrypoint " << entrypoint;
      continue;
    }
    profile_info.va_profile = va_profile;
    supported_profile_infos.push_back(profile_info);
  }
  return supported_profile_infos;
}

bool VaapiWrapper::VaInitialize(const base::Closure& report_error_to_uma_cb) {
  report_error_to_uma_cb_ = report_error_to_uma_cb;
  {
    base::AutoLock auto_lock(*va_lock_);
    if (!VADisplayState::Get()->Initialize())
      return false;
  }

  va_display_ = VADisplayState::Get()->va_display();
  DCHECK(va_display_) << "VADisplayState hasn't been properly Initialize()d";
  return true;
}

bool LazyProfileInfos::GetSupportedVAProfiles(
    std::vector<VAProfile>* profiles) {
  base::AutoLock auto_lock(*va_lock_);
  // Query the driver for supported profiles.
  const int max_profiles = vaMaxNumProfiles(va_display_);
  std::vector<VAProfile> supported_profiles(
      base::checked_cast<size_t>(max_profiles));

  int num_supported_profiles;
  VAStatus va_res = vaQueryConfigProfiles(va_display_, &supported_profiles[0],
                                          &num_supported_profiles);
  VA_SUCCESS_OR_RETURN(va_res, "vaQueryConfigProfiles failed", false);
  if (num_supported_profiles < 0 || num_supported_profiles > max_profiles) {
    LOG(ERROR) << "vaQueryConfigProfiles returned: " << num_supported_profiles;
    return false;
  }

  supported_profiles.resize(base::checked_cast<size_t>(num_supported_profiles));
  *profiles = supported_profiles;
  return true;
}

bool LazyProfileInfos::IsEntrypointSupported_Locked(VAProfile va_profile,
                                                    VAEntrypoint entrypoint) {
  va_lock_->AssertAcquired();
  // Query the driver for supported entrypoints.
  int max_entrypoints = vaMaxNumEntrypoints(va_display_);
  std::vector<VAEntrypoint> supported_entrypoints(
      base::checked_cast<size_t>(max_entrypoints));

  int num_supported_entrypoints;
  VAStatus va_res = vaQueryConfigEntrypoints(va_display_, va_profile,
                                             &supported_entrypoints[0],
                                             &num_supported_entrypoints);
  VA_SUCCESS_OR_RETURN(va_res, "vaQueryConfigEntrypoints failed", false);
  if (num_supported_entrypoints < 0 ||
      num_supported_entrypoints > max_entrypoints) {
    LOG(ERROR) << "vaQueryConfigEntrypoints returned: "
               << num_supported_entrypoints;
    return false;
  }

  return base::ContainsValue(supported_entrypoints, entrypoint);
}

bool LazyProfileInfos::AreAttribsSupported_Locked(
    VAProfile va_profile,
    VAEntrypoint entrypoint,
    const std::vector<VAConfigAttrib>& required_attribs) {
  va_lock_->AssertAcquired();
  // Query the driver for required attributes.
  std::vector<VAConfigAttrib> attribs = required_attribs;
  for (size_t i = 0; i < required_attribs.size(); ++i)
    attribs[i].value = 0;

  VAStatus va_res = vaGetConfigAttributes(va_display_, va_profile, entrypoint,
                                          &attribs[0], attribs.size());
  VA_SUCCESS_OR_RETURN(va_res, "vaGetConfigAttributes failed", false);

  for (size_t i = 0; i < required_attribs.size(); ++i) {
    if (attribs[i].type != required_attribs[i].type ||
        (attribs[i].value & required_attribs[i].value) !=
            required_attribs[i].value) {
      DVLOG(1) << "Unsupported value " << required_attribs[i].value
               << " for attribute type " << required_attribs[i].type;
      return false;
    }
  }
  return true;
}

bool LazyProfileInfos::GetMaxResolution_Locked(
    VAProfile va_profile,
    VAEntrypoint entrypoint,
    std::vector<VAConfigAttrib>& required_attribs,
    gfx::Size* resolution) {
  va_lock_->AssertAcquired();
  VAConfigID va_config_id;
  VAStatus va_res =
      vaCreateConfig(va_display_, va_profile, entrypoint, &required_attribs[0],
                     required_attribs.size(), &va_config_id);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateConfig failed", false);

  // Calls vaQuerySurfaceAttributes twice. The first time is to get the number
  // of attributes to prepare the space and the second time is to get all
  // attributes.
  unsigned int num_attribs;
  va_res = vaQuerySurfaceAttributes(va_display_, va_config_id, nullptr,
                                    &num_attribs);
  VA_SUCCESS_OR_RETURN(va_res, "vaQuerySurfaceAttributes failed", false);
  if (!num_attribs)
    return false;

  std::vector<VASurfaceAttrib> attrib_list(
      base::checked_cast<size_t>(num_attribs));

  va_res = vaQuerySurfaceAttributes(va_display_, va_config_id, &attrib_list[0],
                                    &num_attribs);
  VA_SUCCESS_OR_RETURN(va_res, "vaQuerySurfaceAttributes failed", false);

  resolution->SetSize(0, 0);
  for (const auto& attrib : attrib_list) {
    if (attrib.type == VASurfaceAttribMaxWidth)
      resolution->set_width(attrib.value.value.i);
    else if (attrib.type == VASurfaceAttribMaxHeight)
      resolution->set_height(attrib.value.value.i);
  }
  if (resolution->IsEmpty()) {
    LOG(ERROR) << "Wrong codec resolution: " << resolution->ToString();
    return false;
  }
  return true;
}

bool VaapiWrapper::Initialize(CodecMode mode, VAProfile va_profile) {
  TryToSetVADisplayAttributeToLocalGPU();

  VAEntrypoint entrypoint =
      (mode == kEncode ? VAEntrypointEncSlice : VAEntrypointVLD);
  std::vector<VAConfigAttrib> required_attribs = GetRequiredAttribs(mode);
  base::AutoLock auto_lock(*va_lock_);
  VAStatus va_res =
      vaCreateConfig(va_display_, va_profile, entrypoint, &required_attribs[0],
                     required_attribs.size(), &va_config_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateConfig failed", false);

  return true;
}

void VaapiWrapper::Deinitialize() {
  base::AutoLock auto_lock(*va_lock_);

  if (va_config_id_ != VA_INVALID_ID) {
    VAStatus va_res = vaDestroyConfig(va_display_, va_config_id_);
    VA_LOG_ON_ERROR(va_res, "vaDestroyConfig failed");
  }

  VAStatus va_res = VA_STATUS_SUCCESS;
  VADisplayState::Get()->Deinitialize(&va_res);
  VA_LOG_ON_ERROR(va_res, "vaTerminate failed");

  va_config_id_ = VA_INVALID_ID;
  va_display_ = NULL;
}

bool VaapiWrapper::CreateSurfaces(unsigned int va_format,
                                  const gfx::Size& size,
                                  size_t num_surfaces,
                                  std::vector<VASurfaceID>* va_surfaces) {
  base::AutoLock auto_lock(*va_lock_);
  DVLOG(2) << "Creating " << num_surfaces << " surfaces";

  DCHECK(va_surfaces->empty());
  DCHECK(va_surface_ids_.empty());
  DCHECK_EQ(va_surface_format_, 0u);
  va_surface_ids_.resize(num_surfaces);

  // Allocate surfaces in driver.
  VAStatus va_res =
      vaCreateSurfaces(va_display_, va_format, size.width(), size.height(),
                       &va_surface_ids_[0], va_surface_ids_.size(), NULL, 0);

  VA_LOG_ON_ERROR(va_res, "vaCreateSurfaces failed");
  if (va_res != VA_STATUS_SUCCESS) {
    va_surface_ids_.clear();
    return false;
  }

  // And create a context associated with them.
  va_res = vaCreateContext(va_display_, va_config_id_, size.width(),
                           size.height(), VA_PROGRESSIVE, &va_surface_ids_[0],
                           va_surface_ids_.size(), &va_context_id_);

  VA_LOG_ON_ERROR(va_res, "vaCreateContext failed");
  if (va_res != VA_STATUS_SUCCESS) {
    DestroySurfaces_Locked();
    return false;
  }

  *va_surfaces = va_surface_ids_;
  va_surface_format_ = va_format;
  return true;
}

void VaapiWrapper::DestroySurfaces() {
  base::AutoLock auto_lock(*va_lock_);
  DVLOG(2) << "Destroying " << va_surface_ids_.size() << " surfaces";

  DestroySurfaces_Locked();
}

void VaapiWrapper::DestroySurfaces_Locked() {
  va_lock_->AssertAcquired();

  if (va_context_id_ != VA_INVALID_ID) {
    VAStatus va_res = vaDestroyContext(va_display_, va_context_id_);
    VA_LOG_ON_ERROR(va_res, "vaDestroyContext failed");
  }

  if (!va_surface_ids_.empty()) {
    VAStatus va_res = vaDestroySurfaces(va_display_, &va_surface_ids_[0],
                                        va_surface_ids_.size());
    VA_LOG_ON_ERROR(va_res, "vaDestroySurfaces failed");
  }

  va_surface_ids_.clear();
  va_context_id_ = VA_INVALID_ID;
  va_surface_format_ = 0;
}

scoped_refptr<VASurface> VaapiWrapper::CreateUnownedSurface(
    unsigned int va_format,
    const gfx::Size& size,
    const std::vector<VASurfaceAttrib>& va_attribs) {
  base::AutoLock auto_lock(*va_lock_);

  std::vector<VASurfaceAttrib> attribs(va_attribs);
  VASurfaceID va_surface_id;
  VAStatus va_res =
      vaCreateSurfaces(va_display_, va_format, size.width(), size.height(),
                       &va_surface_id, 1, &attribs[0], attribs.size());

  scoped_refptr<VASurface> va_surface;
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create unowned VASurface",
                       va_surface);

  // This is safe to use Unretained() here, because the VDA takes care
  // of the destruction order. All the surfaces will be destroyed
  // before VaapiWrapper.
  va_surface = new VASurface(
      va_surface_id, size, va_format,
      base::Bind(&VaapiWrapper::DestroyUnownedSurface, base::Unretained(this)));

  return va_surface;
}

#if defined(USE_OZONE)
scoped_refptr<VASurface> VaapiWrapper::CreateVASurfaceForPixmap(
    const scoped_refptr<gfx::NativePixmap>& pixmap) {
  // Create a VASurface for a NativePixmap by importing the underlying dmabufs.
  VASurfaceAttribExternalBuffers va_attrib_extbuf;
  memset(&va_attrib_extbuf, 0, sizeof(va_attrib_extbuf));

  va_attrib_extbuf.pixel_format =
      BufferFormatToVAFourCC(pixmap->GetBufferFormat());
  gfx::Size size = pixmap->GetBufferSize();
  va_attrib_extbuf.width = size.width();
  va_attrib_extbuf.height = size.height();

  size_t num_fds = pixmap->GetDmaBufFdCount();
  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(pixmap->GetBufferFormat());
  if (num_fds == 0 || num_fds > num_planes) {
    LOG(ERROR) << "Invalid number of dmabuf fds: " << num_fds
               << " , planes: " << num_planes;
    return nullptr;
  }

  for (size_t i = 0; i < num_planes; ++i) {
    va_attrib_extbuf.pitches[i] = pixmap->GetDmaBufPitch(i);
    va_attrib_extbuf.offsets[i] = pixmap->GetDmaBufOffset(i);
    DVLOG(4) << "plane " << i << ": pitch: " << va_attrib_extbuf.pitches[i]
             << " offset: " << va_attrib_extbuf.offsets[i];
  }
  va_attrib_extbuf.num_planes = num_planes;

  std::vector<unsigned long> fds(num_fds);
  for (size_t i = 0; i < num_fds; ++i) {
    int dmabuf_fd = pixmap->GetDmaBufFd(i);
    if (dmabuf_fd < 0) {
      LOG(ERROR) << "Failed to get dmabuf from an Ozone NativePixmap";
      return nullptr;
    }
    fds[i] = dmabuf_fd;
  }
  va_attrib_extbuf.buffers = fds.data();
  va_attrib_extbuf.num_buffers = fds.size();

  va_attrib_extbuf.flags = 0;
  va_attrib_extbuf.private_data = NULL;

  std::vector<VASurfaceAttrib> va_attribs(2);

  va_attribs[0].type = VASurfaceAttribMemoryType;
  va_attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[0].value.type = VAGenericValueTypeInteger;
  va_attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

  va_attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
  va_attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[1].value.type = VAGenericValueTypePointer;
  va_attribs[1].value.value.p = &va_attrib_extbuf;

  scoped_refptr<VASurface> va_surface = CreateUnownedSurface(
      BufferFormatToVARTFormat(pixmap->GetBufferFormat()), size, va_attribs);
  if (!va_surface) {
    LOG(ERROR) << "Failed to create VASurface for an Ozone NativePixmap";
    return nullptr;
  }

  return va_surface;
}
#endif

void VaapiWrapper::DestroyUnownedSurface(VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaDestroySurfaces(va_display_, &va_surface_id, 1);
  VA_LOG_ON_ERROR(va_res, "vaDestroySurfaces on surface failed");
}

bool VaapiWrapper::SubmitBuffer(VABufferType va_buffer_type,
                                size_t size,
                                void* buffer) {
  base::AutoLock auto_lock(*va_lock_);

  VABufferID buffer_id;
  VAStatus va_res = vaCreateBuffer(va_display_, va_context_id_, va_buffer_type,
                                   size, 1, buffer, &buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a VA buffer", false);

  switch (va_buffer_type) {
    case VASliceParameterBufferType:
    case VASliceDataBufferType:
    case VAEncSliceParameterBufferType:
      pending_slice_bufs_.push_back(buffer_id);
      break;

    default:
      pending_va_bufs_.push_back(buffer_id);
      break;
  }

  return true;
}

bool VaapiWrapper::SubmitVAEncMiscParamBuffer(
    VAEncMiscParameterType misc_param_type,
    size_t size,
    void* buffer) {
  base::AutoLock auto_lock(*va_lock_);

  VABufferID buffer_id;
  VAStatus va_res = vaCreateBuffer(
      va_display_, va_context_id_, VAEncMiscParameterBufferType,
      sizeof(VAEncMiscParameterBuffer) + size, 1, NULL, &buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a VA buffer", false);

  void* data_ptr = NULL;
  va_res = vaMapBuffer(va_display_, buffer_id, &data_ptr);
  VA_LOG_ON_ERROR(va_res, "vaMapBuffer failed");
  if (va_res != VA_STATUS_SUCCESS) {
    vaDestroyBuffer(va_display_, buffer_id);
    return false;
  }

  DCHECK(data_ptr);

  VAEncMiscParameterBuffer* misc_param =
      reinterpret_cast<VAEncMiscParameterBuffer*>(data_ptr);
  misc_param->type = misc_param_type;
  memcpy(misc_param->data, buffer, size);
  va_res = vaUnmapBuffer(va_display_, buffer_id);
  VA_LOG_ON_ERROR(va_res, "vaUnmapBuffer failed");

  pending_va_bufs_.push_back(buffer_id);
  return true;
}

void VaapiWrapper::DestroyPendingBuffers() {
  base::AutoLock auto_lock(*va_lock_);

  for (const auto& pending_va_buf : pending_va_bufs_) {
    VAStatus va_res = vaDestroyBuffer(va_display_, pending_va_buf);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  for (const auto& pending_slice_buf : pending_slice_bufs_) {
    VAStatus va_res = vaDestroyBuffer(va_display_, pending_slice_buf);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  pending_va_bufs_.clear();
  pending_slice_bufs_.clear();
}

bool VaapiWrapper::CreateCodedBuffer(size_t size, VABufferID* buffer_id) {
  base::AutoLock auto_lock(*va_lock_);
  VAStatus va_res =
      vaCreateBuffer(va_display_, va_context_id_, VAEncCodedBufferType, size, 1,
                     NULL, buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a coded buffer", false);

  const auto is_new_entry = coded_buffers_.insert(*buffer_id).second;
  DCHECK(is_new_entry);
  return true;
}

void VaapiWrapper::DestroyCodedBuffers() {
  base::AutoLock auto_lock(*va_lock_);

  for (std::set<VABufferID>::const_iterator iter = coded_buffers_.begin();
       iter != coded_buffers_.end(); ++iter) {
    VAStatus va_res = vaDestroyBuffer(va_display_, *iter);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  coded_buffers_.clear();
}

bool VaapiWrapper::Execute(VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(*va_lock_);

  DVLOG(4) << "Pending VA bufs to commit: " << pending_va_bufs_.size();
  DVLOG(4) << "Pending slice bufs to commit: " << pending_slice_bufs_.size();
  DVLOG(4) << "Target VA surface " << va_surface_id;

  // Get ready to execute for given surface.
  VAStatus va_res = vaBeginPicture(va_display_, va_context_id_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "vaBeginPicture failed", false);

  if (pending_va_bufs_.size() > 0) {
    // Commit parameter and slice buffers.
    va_res = vaRenderPicture(va_display_, va_context_id_, &pending_va_bufs_[0],
                             pending_va_bufs_.size());
    VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for va_bufs failed", false);
  }

  if (pending_slice_bufs_.size() > 0) {
    va_res =
        vaRenderPicture(va_display_, va_context_id_, &pending_slice_bufs_[0],
                        pending_slice_bufs_.size());
    VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for slices failed", false);
  }

  // Instruct HW codec to start processing committed buffers.
  // Does not block and the job is not finished after this returns.
  va_res = vaEndPicture(va_display_, va_context_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaEndPicture failed", false);

  return true;
}

bool VaapiWrapper::ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id) {
  bool result = Execute(va_surface_id);
  DestroyPendingBuffers();
  return result;
}

#if defined(USE_X11)
bool VaapiWrapper::PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                                        Pixmap x_pixmap,
                                        gfx::Size dest_size) {
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  // Put the data into an X Pixmap.
  va_res = vaPutSurface(va_display_,
                        va_surface_id,
                        x_pixmap,
                        0, 0, dest_size.width(), dest_size.height(),
                        0, 0, dest_size.width(), dest_size.height(),
                        NULL, 0, 0);
  VA_SUCCESS_OR_RETURN(va_res, "Failed putting surface to pixmap", false);
  return true;
}
#endif  // USE_X11

bool VaapiWrapper::GetVaImage(VASurfaceID va_surface_id,
                              VAImageFormat* format,
                              const gfx::Size& size,
                              VAImage* image,
                              void** mem) {
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  va_res =
      vaCreateImage(va_display_, format, size.width(), size.height(), image);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateImage failed", false);

  va_res = vaGetImage(va_display_, va_surface_id, 0, 0, size.width(),
                      size.height(), image->image_id);
  VA_LOG_ON_ERROR(va_res, "vaGetImage failed");

  if (va_res == VA_STATUS_SUCCESS) {
    // Map the VAImage into memory
    va_res = vaMapBuffer(va_display_, image->buf, mem);
    VA_LOG_ON_ERROR(va_res, "vaMapBuffer failed");
  }

  if (va_res != VA_STATUS_SUCCESS) {
    va_res = vaDestroyImage(va_display_, image->image_id);
    VA_LOG_ON_ERROR(va_res, "vaDestroyImage failed");
    return false;
  }

  return true;
}

void VaapiWrapper::ReturnVaImage(VAImage* image) {
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaUnmapBuffer(va_display_, image->buf);
  VA_LOG_ON_ERROR(va_res, "vaUnmapBuffer failed");

  va_res = vaDestroyImage(va_display_, image->image_id);
  VA_LOG_ON_ERROR(va_res, "vaDestroyImage failed");
}

static void DestroyVAImage(VADisplay va_display, VAImage image) {
  if (image.image_id != VA_INVALID_ID)
    vaDestroyImage(va_display, image.image_id);
}

bool VaapiWrapper::UploadVideoFrameToSurface(
    const scoped_refptr<VideoFrame>& frame,
    VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(*va_lock_);

  VAImage image;
  VAStatus va_res = vaDeriveImage(va_display_, va_surface_id, &image);
  VA_SUCCESS_OR_RETURN(va_res, "vaDeriveImage failed", false);
  base::ScopedClosureRunner vaimage_deleter(
      base::Bind(&DestroyVAImage, va_display_, image));

  if (image.format.fourcc != VA_FOURCC_NV12) {
    LOG(ERROR) << "Unsupported image format: " << image.format.fourcc;
    return false;
  }

  if (gfx::Rect(image.width, image.height) < gfx::Rect(frame->coded_size())) {
    LOG(ERROR) << "Buffer too small to fit the frame.";
    return false;
  }

  void* image_ptr = NULL;
  va_res = vaMapBuffer(va_display_, image.buf, &image_ptr);
  VA_SUCCESS_OR_RETURN(va_res, "vaMapBuffer failed", false);
  DCHECK(image_ptr);

  int ret = 0;
  {
    base::AutoUnlock auto_unlock(*va_lock_);
    ret = libyuv::I420ToNV12(
        frame->data(VideoFrame::kYPlane), frame->stride(VideoFrame::kYPlane),
        frame->data(VideoFrame::kUPlane), frame->stride(VideoFrame::kUPlane),
        frame->data(VideoFrame::kVPlane), frame->stride(VideoFrame::kVPlane),
        static_cast<uint8_t*>(image_ptr) + image.offsets[0], image.pitches[0],
        static_cast<uint8_t*>(image_ptr) + image.offsets[1], image.pitches[1],
        image.width, image.height);
  }

  va_res = vaUnmapBuffer(va_display_, image.buf);
  VA_LOG_ON_ERROR(va_res, "vaUnmapBuffer failed");

  return ret == 0;
}

bool VaapiWrapper::DownloadAndDestroyCodedBuffer(VABufferID buffer_id,
                                                 VASurfaceID sync_surface_id,
                                                 uint8_t* target_ptr,
                                                 size_t target_size,
                                                 size_t* coded_data_size) {
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, sync_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  VACodedBufferSegment* buffer_segment = NULL;
  va_res = vaMapBuffer(va_display_, buffer_id,
                       reinterpret_cast<void**>(&buffer_segment));
  VA_SUCCESS_OR_RETURN(va_res, "vaMapBuffer failed", false);
  DCHECK(target_ptr);

  {
    base::AutoUnlock auto_unlock(*va_lock_);
    *coded_data_size = 0;

    while (buffer_segment) {
      DCHECK(buffer_segment->buf);

      if (buffer_segment->size > target_size) {
        LOG(ERROR) << "Insufficient output buffer size";
        break;
      }

      memcpy(target_ptr, buffer_segment->buf, buffer_segment->size);

      target_ptr += buffer_segment->size;
      *coded_data_size += buffer_segment->size;
      target_size -= buffer_segment->size;

      buffer_segment =
          reinterpret_cast<VACodedBufferSegment*>(buffer_segment->next);
    }
  }

  va_res = vaUnmapBuffer(va_display_, buffer_id);
  VA_LOG_ON_ERROR(va_res, "vaUnmapBuffer failed");

  va_res = vaDestroyBuffer(va_display_, buffer_id);
  VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");

  const auto was_found = coded_buffers_.erase(buffer_id);
  DCHECK(was_found);

  return buffer_segment == NULL;
}

bool VaapiWrapper::BlitSurface(
    const scoped_refptr<VASurface>& va_surface_src,
    const scoped_refptr<VASurface>& va_surface_dest) {
  base::AutoLock auto_lock(*va_lock_);

  // Initialize the post processing engine if not already done.
  if (va_vpp_buffer_id_ == VA_INVALID_ID) {
    if (!InitializeVpp_Locked())
      return false;
  }

  VAProcPipelineParameterBuffer* pipeline_param;
  VA_SUCCESS_OR_RETURN(vaMapBuffer(va_display_, va_vpp_buffer_id_,
                                   reinterpret_cast<void**>(&pipeline_param)),
                       "Couldn't map vpp buffer", false);

  memset(pipeline_param, 0, sizeof *pipeline_param);
  const gfx::Size src_size = va_surface_src->size();
  const gfx::Size dest_size = va_surface_dest->size();

  VARectangle input_region;
  input_region.x = input_region.y = 0;
  input_region.width = src_size.width();
  input_region.height = src_size.height();
  pipeline_param->surface_region = &input_region;
  pipeline_param->surface = va_surface_src->id();
  pipeline_param->surface_color_standard = VAProcColorStandardNone;

  VARectangle output_region;
  output_region.x = output_region.y = 0;
  output_region.width = dest_size.width();
  output_region.height = dest_size.height();
  pipeline_param->output_region = &output_region;
  pipeline_param->output_background_color = 0xff000000;
  pipeline_param->output_color_standard = VAProcColorStandardNone;
  pipeline_param->filter_flags = VA_FILTER_SCALING_DEFAULT;

  VA_SUCCESS_OR_RETURN(vaUnmapBuffer(va_display_, va_vpp_buffer_id_),
                       "Couldn't unmap vpp buffer", false);

  VA_SUCCESS_OR_RETURN(
      vaBeginPicture(va_display_, va_vpp_context_id_, va_surface_dest->id()),
      "Couldn't begin picture", false);

  VA_SUCCESS_OR_RETURN(
      vaRenderPicture(va_display_, va_vpp_context_id_, &va_vpp_buffer_id_, 1),
      "Couldn't render picture", false);

  VA_SUCCESS_OR_RETURN(vaEndPicture(va_display_, va_vpp_context_id_),
                       "Couldn't end picture", false);

  return true;
}

bool VaapiWrapper::InitializeVpp_Locked() {
  va_lock_->AssertAcquired();

  VA_SUCCESS_OR_RETURN(
      vaCreateConfig(va_display_, VAProfileNone, VAEntrypointVideoProc, NULL, 0,
                     &va_vpp_config_id_),
      "Couldn't create config", false);

  // The size of the picture for the context is irrelevant in the case
  // of the VPP, just passing 1x1.
  VA_SUCCESS_OR_RETURN(vaCreateContext(va_display_, va_vpp_config_id_, 1, 1, 0,
                                       NULL, 0, &va_vpp_context_id_),
                       "Couldn't create context", false);

  VA_SUCCESS_OR_RETURN(vaCreateBuffer(va_display_, va_vpp_context_id_,
                                      VAProcPipelineParameterBufferType,
                                      sizeof(VAProcPipelineParameterBuffer), 1,
                                      NULL, &va_vpp_buffer_id_),
                       "Couldn't create buffer", false);

  return true;
}

void VaapiWrapper::DeinitializeVpp() {
  base::AutoLock auto_lock(*va_lock_);

  if (va_vpp_buffer_id_ != VA_INVALID_ID) {
    vaDestroyBuffer(va_display_, va_vpp_buffer_id_);
    va_vpp_buffer_id_ = VA_INVALID_ID;
  }
  if (va_vpp_context_id_ != VA_INVALID_ID) {
    vaDestroyContext(va_display_, va_vpp_context_id_);
    va_vpp_context_id_ = VA_INVALID_ID;
  }
  if (va_vpp_config_id_ != VA_INVALID_ID) {
    vaDestroyConfig(va_display_, va_vpp_config_id_);
    va_vpp_config_id_ = VA_INVALID_ID;
  }
}

// static
void VaapiWrapper::PreSandboxInitialization() {
  VADisplayState::PreSandboxInitialization();
}

// static
LazyProfileInfos* LazyProfileInfos::Get() {
  static LazyProfileInfos* profile_infos = new LazyProfileInfos();
  return profile_infos;
}

LazyProfileInfos::LazyProfileInfos()
    : va_lock_(VADisplayState::Get()->va_lock()),
      va_display_(nullptr),
      report_error_to_uma_cb_(base::Bind(&base::DoNothing)) {
  static_assert(arraysize(supported_profiles_) == VaapiWrapper::kCodecModeMax,
                "The array size of supported profile is incorrect.");

  static bool result = VADisplayState::PostSandboxInitialization();
  if (!result)
    return;
  {
    base::AutoLock auto_lock(*va_lock_);
    if (!VADisplayState::Get()->Initialize())
      return;
  }

  va_display_ = VADisplayState::Get()->va_display();
  DCHECK(va_display_) << "VADisplayState hasn't been properly Initialize()d";

  for (size_t i = 0; i < VaapiWrapper::kCodecModeMax; ++i) {
    supported_profiles_[i] = GetSupportedProfileInfosForCodecModeInternal(
        static_cast<VaapiWrapper::CodecMode>(i));
  }
}

LazyProfileInfos::~LazyProfileInfos() {
  VAStatus va_res = VA_STATUS_SUCCESS;
  VADisplayState::Get()->Deinitialize(&va_res);
  VA_LOG_ON_ERROR(va_res, "vaTerminate failed");
  va_display_ = nullptr;
}

std::vector<LazyProfileInfos::ProfileInfo>
LazyProfileInfos::GetSupportedProfileInfosForCodecMode(
    VaapiWrapper::CodecMode mode) {
  return supported_profiles_[mode];
}

bool LazyProfileInfos::IsProfileSupported(VaapiWrapper::CodecMode mode,
                                          VAProfile va_profile) {
  for (const auto& profile : supported_profiles_[mode]) {
    if (profile.va_profile == va_profile)
      return true;
  }
  return false;
}

}  // namespace media
