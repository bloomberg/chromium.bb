// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/library_cdm/clear_key_cdm/clear_key_cdm.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/cdm/api/content_decryption_module_ext.h"
#include "media/cdm/json_web_key.h"
#include "media/cdm/library_cdm/cdm_host_proxy.h"
#include "media/cdm/library_cdm/cdm_host_proxy_impl.h"
#include "media/cdm/library_cdm/clear_key_cdm/cdm_file_io_test.h"
#include "media/cdm/library_cdm/clear_key_cdm/cdm_proxy_test.h"
#include "media/cdm/library_cdm/clear_key_cdm/cdm_video_decoder.h"
#include "media/media_buildflags.h"

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "media/base/media.h"
#include "media/cdm/library_cdm/clear_key_cdm/ffmpeg_cdm_audio_decoder.h"
#include "media/cdm/library_cdm/clear_key_cdm/ffmpeg_cdm_video_decoder.h"

#if !defined COMPONENT_BUILD
static base::AtExitManager g_at_exit_manager;
#endif
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER

const char kClearKeyCdmVersion[] = "0.1.0.1";
const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";

// Variants of External Clear Key key system to test different scenarios.
const char kExternalClearKeyDecryptOnlyKeySystem[] =
    "org.chromium.externalclearkey.decryptonly";
const char kExternalClearKeyMessageTypeTestKeySystem[] =
    "org.chromium.externalclearkey.messagetypetest";
const char kExternalClearKeyFileIOTestKeySystem[] =
    "org.chromium.externalclearkey.fileiotest";
const char kExternalClearKeyOutputProtectionTestKeySystem[] =
    "org.chromium.externalclearkey.outputprotectiontest";
const char kExternalClearKeyPlatformVerificationTestKeySystem[] =
    "org.chromium.externalclearkey.platformverificationtest";
const char kExternalClearKeyCrashKeySystem[] =
    "org.chromium.externalclearkey.crash";
const char kExternalClearKeyVerifyCdmHostTestKeySystem[] =
    "org.chromium.externalclearkey.verifycdmhosttest";
const char kExternalClearKeyStorageIdTestKeySystem[] =
    "org.chromium.externalclearkey.storageidtest";
const char kExternalClearKeyDifferentGuidTestKeySystem[] =
    "org.chromium.externalclearkey.differentguid";
const char kExternalClearKeyCdmProxyTestKeySystem[] =
    "org.chromium.externalclearkey.cdmproxytest";

const int64_t kSecondsPerMinute = 60;
const int64_t kMsPerSecond = 1000;
const int64_t kMaxTimerDelayMs = 1 * kSecondsPerMinute * kMsPerSecond;

// CDM unit test result header. Must be in sync with UNIT_TEST_RESULT_HEADER in
// media/test/data/eme_player_js/globals.js.
const char kUnitTestResultHeader[] = "UNIT_TEST_RESULT";

const char kDummyIndividualizationRequest[] = "dummy individualization request";

static bool g_is_cdm_module_initialized = false;

// Copies |input_buffer| into a DecoderBuffer. If the |input_buffer| is
// empty, an empty (end-of-stream) DecoderBuffer is returned.
static scoped_refptr<media::DecoderBuffer> CopyDecoderBufferFrom(
    const cdm::InputBuffer_2& input_buffer) {
  if (!input_buffer.data) {
    DCHECK(!input_buffer.data_size);
    return media::DecoderBuffer::CreateEOSBuffer();
  }

  // TODO(xhwang): Get rid of this copy.
  scoped_refptr<media::DecoderBuffer> output_buffer =
      media::DecoderBuffer::CopyFrom(input_buffer.data, input_buffer.data_size);
  output_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(input_buffer.timestamp));

  // TODO(crbug.com/658026): Support other schemes.
  if (input_buffer.encryption_scheme == cdm::EncryptionScheme::kCenc) {
    DCHECK_GT(input_buffer.iv_size, 0u);
    DCHECK_GT(input_buffer.key_id_size, 0u);
    std::vector<media::SubsampleEntry> subsamples;
    for (uint32_t i = 0; i < input_buffer.num_subsamples; ++i) {
      subsamples.push_back(
          media::SubsampleEntry(input_buffer.subsamples[i].clear_bytes,
                                input_buffer.subsamples[i].cipher_bytes));
    }

    output_buffer->set_decrypt_config(media::DecryptConfig::CreateCencConfig(
        std::string(reinterpret_cast<const char*>(input_buffer.key_id),
                    input_buffer.key_id_size),
        std::string(reinterpret_cast<const char*>(input_buffer.iv),
                    input_buffer.iv_size),
        subsamples));
  }

  return output_buffer;
}

static std::string GetUnitTestResultMessage(bool success) {
  std::string message(kUnitTestResultHeader);
  message += success ? '1' : '0';
  return message;
}

static cdm::Exception ConvertException(
    media::CdmPromise::Exception exception_code) {
  switch (exception_code) {
    case media::CdmPromise::Exception::NOT_SUPPORTED_ERROR:
      return cdm::kExceptionNotSupportedError;
    case media::CdmPromise::Exception::INVALID_STATE_ERROR:
      return cdm::kExceptionInvalidStateError;
    case media::CdmPromise::Exception::TYPE_ERROR:
      return cdm::kExceptionTypeError;
    case media::CdmPromise::Exception::QUOTA_EXCEEDED_ERROR:
      return cdm::kExceptionQuotaExceededError;
  }
  NOTREACHED();
  return cdm::kExceptionInvalidStateError;
}

static media::CdmSessionType ConvertSessionType(cdm::SessionType session_type) {
  switch (session_type) {
    case cdm::kTemporary:
      return media::CdmSessionType::TEMPORARY_SESSION;
    case cdm::kPersistentLicense:
      return media::CdmSessionType::PERSISTENT_LICENSE_SESSION;
    case cdm::kPersistentKeyRelease:
      return media::CdmSessionType::PERSISTENT_RELEASE_MESSAGE_SESSION;
  }
  NOTREACHED();
  return media::CdmSessionType::TEMPORARY_SESSION;
}

static media::EmeInitDataType ConvertInitDataType(
    cdm::InitDataType init_data_type) {
  switch (init_data_type) {
    case cdm::kCenc:
      return media::EmeInitDataType::CENC;
    case cdm::kKeyIds:
      return media::EmeInitDataType::KEYIDS;
    case cdm::kWebM:
      return media::EmeInitDataType::WEBM;
  }
  NOTREACHED();
  return media::EmeInitDataType::UNKNOWN;
}

cdm::KeyStatus ConvertKeyStatus(media::CdmKeyInformation::KeyStatus status) {
  switch (status) {
    case media::CdmKeyInformation::KeyStatus::USABLE:
      return cdm::kUsable;
    case media::CdmKeyInformation::KeyStatus::INTERNAL_ERROR:
      return cdm::kInternalError;
    case media::CdmKeyInformation::KeyStatus::EXPIRED:
      return cdm::kExpired;
    case media::CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED:
      return cdm::kOutputRestricted;
    case media::CdmKeyInformation::KeyStatus::OUTPUT_DOWNSCALED:
      return cdm::kOutputDownscaled;
    case media::CdmKeyInformation::KeyStatus::KEY_STATUS_PENDING:
      return cdm::kStatusPending;
    case media::CdmKeyInformation::KeyStatus::RELEASED:
      return cdm::kReleased;
  }
  NOTREACHED();
  return cdm::kInternalError;
}

cdm::MessageType ConvertMessageType(media::CdmMessageType message_type) {
  switch (message_type) {
    case media::CdmMessageType::LICENSE_REQUEST:
      return cdm::kLicenseRequest;
    case media::CdmMessageType::LICENSE_RENEWAL:
      return cdm::kLicenseRenewal;
    case media::CdmMessageType::LICENSE_RELEASE:
      return cdm::kLicenseRelease;
    case media::CdmMessageType::INDIVIDUALIZATION_REQUEST:
      return cdm::kIndividualizationRequest;
  }

  NOTREACHED();
  return cdm::kLicenseRequest;
}

// Shallow copy all the key information from |keys_info| into |keys_vector|.
// |keys_vector| is only valid for the lifetime of |keys_info| because it
// contains pointers into the latter.
void ConvertCdmKeysInfo(const media::CdmKeysInfo& keys_info,
                        std::vector<cdm::KeyInformation>* keys_vector) {
  keys_vector->reserve(keys_info.size());
  for (const auto& key_info : keys_info) {
    cdm::KeyInformation key = {};
    key.key_id = key_info->key_id.data();
    key.key_id_size = key_info->key_id.size();
    key.status = ConvertKeyStatus(key_info->status);
    key.system_code = key_info->system_code;
    keys_vector->push_back(key);
  }
}

void INITIALIZE_CDM_MODULE() {
  DVLOG(1) << __func__;
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  media::InitializeMediaLibrary();
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER

  g_is_cdm_module_initialized = true;
}

void DeinitializeCdmModule() {
  DVLOG(1) << __func__;
}

void* CreateCdmInstance(int cdm_interface_version,
                        const char* key_system,
                        uint32_t key_system_size,
                        GetCdmHostFunc get_cdm_host_func,
                        void* user_data) {
  DVLOG(1) << "CreateCdmInstance()";

  if (!g_is_cdm_module_initialized) {
    DVLOG(1) << "CDM module not initialized.";
    return nullptr;
  }

  std::string key_system_string(key_system, key_system_size);
  if (key_system_string != kExternalClearKeyKeySystem &&
      key_system_string != kExternalClearKeyDecryptOnlyKeySystem &&
      key_system_string != kExternalClearKeyMessageTypeTestKeySystem &&
      key_system_string != kExternalClearKeyFileIOTestKeySystem &&
      key_system_string != kExternalClearKeyOutputProtectionTestKeySystem &&
      key_system_string != kExternalClearKeyPlatformVerificationTestKeySystem &&
      key_system_string != kExternalClearKeyCrashKeySystem &&
      key_system_string != kExternalClearKeyVerifyCdmHostTestKeySystem &&
      key_system_string != kExternalClearKeyStorageIdTestKeySystem &&
      key_system_string != kExternalClearKeyDifferentGuidTestKeySystem &&
      key_system_string != kExternalClearKeyCdmProxyTestKeySystem) {
    DVLOG(1) << "Unsupported key system:" << key_system_string;
    return nullptr;
  }

  // We support CDM_9, CDM_10 and CDM_11.
  using CDM_9 = cdm::ContentDecryptionModule_9;
  using CDM_10 = cdm::ContentDecryptionModule_10;
  using CDM_11 = cdm::ContentDecryptionModule_11;

  if (cdm_interface_version == CDM_9::kVersion) {
    CDM_9::Host* host = static_cast<CDM_9::Host*>(
        get_cdm_host_func(CDM_9::Host::kVersion, user_data));
    if (!host)
      return nullptr;

    DVLOG(1) << __func__ << ": Create ClearKeyCdm with CDM_9::Host.";
    return static_cast<CDM_9*>(new media::ClearKeyCdm(host, key_system_string));
  }

  if (cdm_interface_version == CDM_10::kVersion) {
    CDM_10::Host* host = static_cast<CDM_10::Host*>(
        get_cdm_host_func(CDM_10::Host::kVersion, user_data));
    if (!host)
      return nullptr;

    DVLOG(1) << __func__ << ": Create ClearKeyCdm with CDM_10::Host.";
    return static_cast<CDM_10*>(
        new media::ClearKeyCdm(host, key_system_string));
  }

  if (cdm_interface_version == CDM_11::kVersion) {
    CDM_11::Host* host = static_cast<CDM_11::Host*>(
        get_cdm_host_func(CDM_11::Host::kVersion, user_data));
    if (!host)
      return nullptr;

    DVLOG(1) << __func__ << ": Create ClearKeyCdm with CDM_11::Host.";
    return static_cast<CDM_11*>(
        new media::ClearKeyCdm(host, key_system_string));
  }

  return nullptr;
}

const char* GetCdmVersion() {
  return kClearKeyCdmVersion;
}

static bool g_verify_host_files_result = false;

// Makes sure files and corresponding signature files are readable but not
// writable.
bool VerifyCdmHost_0(const cdm::HostFile* host_files, uint32_t num_files) {
  DVLOG(1) << __func__ << ": " << num_files;

  // We should always have the CDM and at least one common file.
  // The common CDM host file (e.g. chrome) might not exist since we are running
  // in browser_tests.
  const uint32_t kMinNumHostFiles = 2;

  // We should always have the CDM.
  const int kNumCdmFiles = 1;

  if (num_files < kMinNumHostFiles) {
    LOG(ERROR) << "Too few host files: " << num_files;
    g_verify_host_files_result = false;
    return true;
  }

  int num_opened_files = 0;
  for (uint32_t i = 0; i < num_files; ++i) {
    const int kBytesToRead = 10;
    std::vector<char> buffer(kBytesToRead);

    base::File file(static_cast<base::PlatformFile>(host_files[i].file));
    if (!file.IsValid())
      continue;

    num_opened_files++;

    int bytes_read = file.Read(0, buffer.data(), buffer.size());
    if (bytes_read != kBytesToRead) {
      LOG(ERROR) << "File bytes read: " << bytes_read;
      g_verify_host_files_result = false;
      return true;
    }

    // TODO(xhwang): Check that the files are not writable.
    // TODO(xhwang): Also verify the signature file when it's available.
  }

  // We should always have CDM files opened.
  if (num_opened_files < kNumCdmFiles) {
    LOG(ERROR) << "Too few opened files: " << num_opened_files;
    g_verify_host_files_result = false;
    return true;
  }

  g_verify_host_files_result = true;
  return true;
}

namespace media {

namespace {

bool IsSupportedConfigEncryptionScheme(cdm::EncryptionScheme scheme) {
  // TODO(crbug.com/658026): Support other decryption schemes.
  switch (scheme) {
    case cdm::EncryptionScheme::kUnencrypted:
    case cdm::EncryptionScheme::kCenc:
      return true;
    case cdm::EncryptionScheme::kCbcs:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsSupportedBufferEncryptionScheme(cdm::EncryptionScheme scheme,
                                       cdm::Pattern pattern) {
  // TODO(crbug.com/658026): Support other decryption schemes.
  switch (scheme) {
    case cdm::EncryptionScheme::kUnencrypted:
      return true;
    case cdm::EncryptionScheme::kCenc:
      return pattern.crypt_byte_block == 0 && pattern.skip_byte_block == 0;
    case cdm::EncryptionScheme::kCbcs:
      return false;
  }

  NOTREACHED();
  return false;
}

cdm::InputBuffer_2 ToInputBuffer_2(cdm::InputBuffer_1 encrypted_buffer) {
  cdm::InputBuffer_2 buffer = {};
  buffer.data = encrypted_buffer.data;
  buffer.data_size = encrypted_buffer.data_size;
  buffer.key_id = encrypted_buffer.key_id;
  buffer.key_id_size = encrypted_buffer.key_id_size;
  buffer.iv = encrypted_buffer.iv;
  buffer.iv_size = encrypted_buffer.iv_size;
  buffer.subsamples = encrypted_buffer.subsamples;
  buffer.num_subsamples = encrypted_buffer.num_subsamples;
  buffer.timestamp = encrypted_buffer.timestamp;
  // InputBuffer_1 must be either 'cenc' or unencrypted.
  buffer.encryption_scheme = (buffer.iv_size == 0)
                                 ? cdm::EncryptionScheme::kUnencrypted
                                 : cdm::EncryptionScheme::kCenc;
  buffer.pattern = {0, 0};
  return buffer;
}

}  // namespace

template <typename HostInterface>
ClearKeyCdm::ClearKeyCdm(HostInterface* host, const std::string& key_system)
    : host_interface_version_(HostInterface::kVersion),
      cdm_host_proxy_(new CdmHostProxyImpl<HostInterface>(host)),
      cdm_(new ClearKeyPersistentSessionCdm(
          cdm_host_proxy_.get(),
          base::Bind(&ClearKeyCdm::OnSessionMessage, base::Unretained(this)),
          base::Bind(&ClearKeyCdm::OnSessionClosed, base::Unretained(this)),
          base::Bind(&ClearKeyCdm::OnSessionKeysChange, base::Unretained(this)),
          base::Bind(&ClearKeyCdm::OnSessionExpirationUpdate,
                     base::Unretained(this)))),
      key_system_(key_system) {
  DCHECK(g_is_cdm_module_initialized);
}

ClearKeyCdm::~ClearKeyCdm() = default;

void ClearKeyCdm::Initialize(bool allow_distinctive_identifier,
                             bool allow_persistent_state) {
  // Implementation doesn't use distinctive identifier and will only need
  // to check persistent state permission.
  allow_persistent_state_ = allow_persistent_state;

  // CdmProxy must be created during initialization time. OnInitialized() will
  // be called in OnCdmProxyTestComplete().
  if (key_system_ == kExternalClearKeyCdmProxyTestKeySystem) {
    StartCdmProxyTest();
    return;
  }

  cdm_host_proxy_->OnInitialized(true);
}

void ClearKeyCdm::Initialize(bool allow_distinctive_identifier,
                             bool allow_persistent_state,
                             bool use_hw_secure_codecs) {
  Initialize(allow_distinctive_identifier, allow_persistent_state);
}

void ClearKeyCdm::GetStatusForPolicy(uint32_t promise_id,
                                     const cdm::Policy& policy) {
  // Pretend the device is HDCP 2.0 compliant.
  const cdm::HdcpVersion kDeviceHdcpVersion = cdm::kHdcpVersion2_0;

  if (policy.min_hdcp_version <= kDeviceHdcpVersion) {
    cdm_host_proxy_->OnResolveKeyStatusPromise(promise_id, cdm::kUsable);
    return;
  }

  cdm_host_proxy_->OnResolveKeyStatusPromise(promise_id,
                                             cdm::kOutputRestricted);
}

void ClearKeyCdm::CreateSessionAndGenerateRequest(
    uint32_t promise_id,
    cdm::SessionType session_type,
    cdm::InitDataType init_data_type,
    const uint8_t* init_data,
    uint32_t init_data_size) {
  DVLOG(1) << __func__;

  if (session_type != cdm::kTemporary && !allow_persistent_state_) {
    OnPromiseFailed(promise_id, CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "Persistent state not allowed.");
    return;
  }

  std::unique_ptr<media::NewSessionCdmPromise> promise(
      new media::CdmCallbackPromise<std::string>(
          base::Bind(&ClearKeyCdm::OnSessionCreated, base::Unretained(this),
                     promise_id),
          base::Bind(&ClearKeyCdm::OnPromiseFailed, base::Unretained(this),
                     promise_id)));
  cdm_->CreateSessionAndGenerateRequest(
      ConvertSessionType(session_type), ConvertInitDataType(init_data_type),
      std::vector<uint8_t>(init_data, init_data + init_data_size),
      std::move(promise));

  // Run unit tests if applicable. Unit test results are reported in the form of
  // a session message. Therefore it can only be called after session creation.
  if (key_system_ == kExternalClearKeyFileIOTestKeySystem) {
    StartFileIOTest();
  } else if (key_system_ == kExternalClearKeyOutputProtectionTestKeySystem) {
    StartOutputProtectionTest();
  } else if (key_system_ ==
             kExternalClearKeyPlatformVerificationTestKeySystem) {
    StartPlatformVerificationTest();
  } else if (key_system_ == kExternalClearKeyVerifyCdmHostTestKeySystem) {
    ReportVerifyCdmHostTestResult();
  } else if (key_system_ == kExternalClearKeyStorageIdTestKeySystem) {
    StartStorageIdTest();
  } else if (key_system_ == kExternalClearKeyCdmProxyTestKeySystem) {
    ReportCdmProxyTestResult();
  }
}

void ClearKeyCdm::LoadSession(uint32_t promise_id,
                              cdm::SessionType session_type,
                              const char* session_id,
                              uint32_t session_id_length) {
  DVLOG(1) << __func__;
  DCHECK_EQ(session_type, cdm::kPersistentLicense);
  DCHECK(allow_persistent_state_);
  std::string web_session_str(session_id, session_id_length);

  std::unique_ptr<media::NewSessionCdmPromise> promise(
      new media::CdmCallbackPromise<std::string>(
          base::Bind(&ClearKeyCdm::OnSessionCreated, base::Unretained(this),
                     promise_id),
          base::Bind(&ClearKeyCdm::OnPromiseFailed, base::Unretained(this),
                     promise_id)));
  cdm_->LoadSession(ConvertSessionType(session_type), web_session_str,
                    std::move(promise));
}

void ClearKeyCdm::UpdateSession(uint32_t promise_id,
                                const char* session_id,
                                uint32_t session_id_length,
                                const uint8_t* response,
                                uint32_t response_size) {
  DVLOG(1) << __func__;
  std::string web_session_str(session_id, session_id_length);

  std::unique_ptr<media::SimpleCdmPromise> promise(
      new media::CdmCallbackPromise<>(
          base::Bind(&ClearKeyCdm::OnUpdateSuccess, base::Unretained(this),
                     promise_id, web_session_str),
          base::Bind(&ClearKeyCdm::OnPromiseFailed, base::Unretained(this),
                     promise_id)));
  cdm_->UpdateSession(web_session_str,
                      std::vector<uint8_t>(response, response + response_size),
                      std::move(promise));
}

void ClearKeyCdm::OnUpdateSuccess(uint32_t promise_id,
                                  const std::string& session_id) {
  // Now create the expiration changed event.
  cdm::Time expiration = 0.0;  // Never expires.

  if (key_system_ == kExternalClearKeyMessageTypeTestKeySystem) {
    // For renewal key system, set a non-zero expiration that is approximately
    // 100 years after 01 January 1970 UTC.
    expiration = 3153600000.0;  // 100 * 365 * 24 * 60 * 60;

    if (!has_set_renewal_timer_) {
      // Make sure the CDM can get time and sleep if necessary.
      constexpr auto kSleepDuration = base::TimeDelta::FromSeconds(1);
      auto start_time = base::Time::Now();
      base::PlatformThread::Sleep(kSleepDuration);
      auto time_elapsed = base::Time::Now() - start_time;
      CHECK_GE(time_elapsed, kSleepDuration);

      ScheduleNextRenewal();
      has_set_renewal_timer_ = true;
    }

    // Also send an individualization request if never sent before. Only
    // supported on Host_10 and later.
    if (host_interface_version_ >= cdm::Host_10::kVersion &&
        !has_sent_individualization_request_) {
      has_sent_individualization_request_ = true;
      const std::string request = kDummyIndividualizationRequest;
      cdm_host_proxy_->OnSessionMessage(session_id.data(), session_id.length(),
                                        cdm::kIndividualizationRequest,
                                        request.data(), request.size());
    }
  }

  cdm_host_proxy_->OnExpirationChange(session_id.data(), session_id.length(),
                                      expiration);

  // Resolve the promise.
  OnPromiseResolved(promise_id);
}

void ClearKeyCdm::CloseSession(uint32_t promise_id,
                               const char* session_id,
                               uint32_t session_id_length) {
  DVLOG(1) << __func__;
  std::string web_session_str(session_id, session_id_length);

  std::unique_ptr<media::SimpleCdmPromise> promise(
      new media::CdmCallbackPromise<>(
          base::Bind(&ClearKeyCdm::OnPromiseResolved, base::Unretained(this),
                     promise_id),
          base::Bind(&ClearKeyCdm::OnPromiseFailed, base::Unretained(this),
                     promise_id)));
  cdm_->CloseSession(web_session_str, std::move(promise));
}

void ClearKeyCdm::RemoveSession(uint32_t promise_id,
                                const char* session_id,
                                uint32_t session_id_length) {
  DVLOG(1) << __func__;
  std::string web_session_str(session_id, session_id_length);

  std::unique_ptr<media::SimpleCdmPromise> promise(
      new media::CdmCallbackPromise<>(
          base::Bind(&ClearKeyCdm::OnPromiseResolved, base::Unretained(this),
                     promise_id),
          base::Bind(&ClearKeyCdm::OnPromiseFailed, base::Unretained(this),
                     promise_id)));
  cdm_->RemoveSession(web_session_str, std::move(promise));
}

void ClearKeyCdm::SetServerCertificate(uint32_t promise_id,
                                       const uint8_t* server_certificate_data,
                                       uint32_t server_certificate_data_size) {
  DVLOG(1) << __func__;
  std::unique_ptr<media::SimpleCdmPromise> promise(
      new media::CdmCallbackPromise<>(
          base::Bind(&ClearKeyCdm::OnPromiseResolved, base::Unretained(this),
                     promise_id),
          base::Bind(&ClearKeyCdm::OnPromiseFailed, base::Unretained(this),
                     promise_id)));
  cdm_->SetServerCertificate(
      std::vector<uint8_t>(
          server_certificate_data,
          server_certificate_data + server_certificate_data_size),
      std::move(promise));
}

void ClearKeyCdm::TimerExpired(void* context) {
  DVLOG(1) << __func__;
  DCHECK(has_set_renewal_timer_);
  std::string renewal_message;
  if (!next_renewal_message_.empty() && context == &next_renewal_message_[0]) {
    renewal_message = next_renewal_message_;
  } else {
    renewal_message = "ERROR: Invalid timer context found!";
  }

  cdm_host_proxy_->OnSessionMessage(
      last_session_id_.data(), last_session_id_.length(), cdm::kLicenseRenewal,
      renewal_message.data(), renewal_message.length());

  ScheduleNextRenewal();
}

static void CopyDecryptResults(media::Decryptor::Status* status_copy,
                               scoped_refptr<DecoderBuffer>* buffer_copy,
                               media::Decryptor::Status status,
                               scoped_refptr<DecoderBuffer> buffer) {
  *status_copy = status;
  *buffer_copy = std::move(buffer);
}

cdm::Status ClearKeyCdm::Decrypt(const cdm::InputBuffer_1& encrypted_buffer,
                                 cdm::DecryptedBlock* decrypted_block) {
  return Decrypt(ToInputBuffer_2(encrypted_buffer), decrypted_block);
}

cdm::Status ClearKeyCdm::Decrypt(const cdm::InputBuffer_2& encrypted_buffer,
                                 cdm::DecryptedBlock* decrypted_block) {
  DVLOG(1) << __func__;
  DCHECK(encrypted_buffer.data);

  scoped_refptr<DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

  DCHECK(buffer->data());
  decrypted_block->SetDecryptedBuffer(
      cdm_host_proxy_->Allocate(buffer->data_size()));
  memcpy(reinterpret_cast<void*>(decrypted_block->DecryptedBuffer()->Data()),
         buffer->data(), buffer->data_size());
  decrypted_block->DecryptedBuffer()->SetSize(buffer->data_size());
  decrypted_block->SetTimestamp(buffer->timestamp().InMicroseconds());

  return cdm::kSuccess;
}

cdm::Status ClearKeyCdm::InitializeAudioDecoder(
    const cdm::AudioDecoderConfig_1& audio_decoder_config) {
  // AudioDecoderConfig_1 doesn't specify the encryption scheme, but only
  // supports 'cenc' or unencrypted media, so expect encrypted audio.
  cdm::AudioDecoderConfig_2 audio_config = {
      audio_decoder_config.codec,
      audio_decoder_config.channel_count,
      audio_decoder_config.bits_per_channel,
      audio_decoder_config.samples_per_second,
      audio_decoder_config.extra_data,
      audio_decoder_config.extra_data_size,
      cdm::EncryptionScheme::kCenc};
  return InitializeAudioDecoder(audio_config);
}

cdm::Status ClearKeyCdm::InitializeAudioDecoder(
    const cdm::AudioDecoderConfig_2& audio_decoder_config) {
  if (key_system_ == kExternalClearKeyDecryptOnlyKeySystem)
    return cdm::kInitializationError;

  if (!IsSupportedConfigEncryptionScheme(
          audio_decoder_config.encryption_scheme)) {
    return cdm::kInitializationError;
  }

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  if (!audio_decoder_)
    audio_decoder_.reset(
        new media::FFmpegCdmAudioDecoder(cdm_host_proxy_.get()));

  if (!audio_decoder_->Initialize(audio_decoder_config))
    return cdm::kInitializationError;

  return cdm::kSuccess;
#else
  NOTIMPLEMENTED();
  return cdm::kSessionError;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

cdm::Status ClearKeyCdm::InitializeVideoDecoder(
    const cdm::VideoDecoderConfig_1& video_decoder_config) {
  // VideoDecoderConfig_1 doesn't specify the encryption scheme, but only
  // supports 'cenc' or unencrypted media, so expect encrypted video.
  cdm::VideoDecoderConfig_2 video_config = {
      video_decoder_config.codec,      video_decoder_config.profile,
      video_decoder_config.format,     video_decoder_config.coded_size,
      video_decoder_config.extra_data, video_decoder_config.extra_data_size,
      cdm::EncryptionScheme::kCenc};
  return InitializeVideoDecoder(video_config);
}

cdm::Status ClearKeyCdm::InitializeVideoDecoder(
    const cdm::VideoDecoderConfig_2& video_decoder_config) {
  if (key_system_ == kExternalClearKeyDecryptOnlyKeySystem)
    return cdm::kInitializationError;

  if (!IsSupportedConfigEncryptionScheme(
          video_decoder_config.encryption_scheme)) {
    return cdm::kInitializationError;
  }

  if (video_decoder_ && video_decoder_->is_initialized()) {
    DCHECK(!video_decoder_->is_initialized());
    return cdm::kInitializationError;
  }

  // Any uninitialized decoder will be replaced.
  video_decoder_ =
      CreateVideoDecoder(cdm_host_proxy_.get(), video_decoder_config);
  if (!video_decoder_)
    return cdm::kInitializationError;

  return cdm::kSuccess;
}

void ClearKeyCdm::ResetDecoder(cdm::StreamType decoder_type) {
  DVLOG(1) << __func__;
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  switch (decoder_type) {
    case cdm::kStreamTypeVideo:
      video_decoder_->Reset();
      break;
    case cdm::kStreamTypeAudio:
      audio_decoder_->Reset();
      break;
    default:
      NOTREACHED() << "ResetDecoder(): invalid cdm::StreamType";
  }
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

void ClearKeyCdm::DeinitializeDecoder(cdm::StreamType decoder_type) {
  DVLOG(1) << __func__;
  switch (decoder_type) {
    case cdm::kStreamTypeVideo:
      video_decoder_->Deinitialize();
      break;
    case cdm::kStreamTypeAudio:
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
      audio_decoder_->Deinitialize();
#endif
      break;
    default:
      NOTREACHED() << "DeinitializeDecoder(): invalid cdm::StreamType";
  }
}

cdm::Status ClearKeyCdm::DecryptAndDecodeFrame(
    const cdm::InputBuffer_1& encrypted_buffer,
    cdm::VideoFrame* decoded_frame) {
  return DecryptAndDecodeFrame(ToInputBuffer_2(encrypted_buffer),
                               decoded_frame);
}

cdm::Status ClearKeyCdm::DecryptAndDecodeFrame(
    const cdm::InputBuffer_2& encrypted_buffer,
    cdm::VideoFrame* decoded_frame) {
  DVLOG(1) << __func__;
  TRACE_EVENT0("media", "ClearKeyCdm::DecryptAndDecodeFrame");

  scoped_refptr<DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

  const uint8_t* data = NULL;
  int32_t size = 0;
  int64_t timestamp = 0;
  if (!buffer->end_of_stream()) {
    data = buffer->data();
    size = buffer->data_size();
    timestamp = encrypted_buffer.timestamp;
  }

  return video_decoder_->DecodeFrame(data, size, timestamp, decoded_frame);
}

cdm::Status ClearKeyCdm::DecryptAndDecodeSamples(
    const cdm::InputBuffer_1& encrypted_buffer,
    cdm::AudioFrames* audio_frames) {
  return DecryptAndDecodeSamples(ToInputBuffer_2(encrypted_buffer),
                                 audio_frames);
}

cdm::Status ClearKeyCdm::DecryptAndDecodeSamples(
    const cdm::InputBuffer_2& encrypted_buffer,
    cdm::AudioFrames* audio_frames) {
  DVLOG(1) << __func__;

  // Trigger a crash on purpose for testing purpose.
  // Only do this after a session has been created since the test also checks
  // that the session is properly closed.
  if (!last_session_id_.empty() &&
      key_system_ == kExternalClearKeyCrashKeySystem) {
    CHECK(false) << "Crash in decrypt-and-decode with crash key system.";
  }

  scoped_refptr<DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  const uint8_t* data = NULL;
  int32_t size = 0;
  int64_t timestamp = 0;
  if (!buffer->end_of_stream()) {
    data = buffer->data();
    size = buffer->data_size();
    timestamp = encrypted_buffer.timestamp;
  }

  return audio_decoder_->DecodeBuffer(data, size, timestamp, audio_frames);
#else
  return cdm::kSuccess;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

void ClearKeyCdm::Destroy() {
  DVLOG(1) << __func__;
  delete this;
}

void ClearKeyCdm::ScheduleNextRenewal() {
  // Prepare the next renewal message and set timer.
  std::ostringstream msg_stream;
  msg_stream << "Renewal from ClearKey CDM set at time "
             << base::Time::FromDoubleT(cdm_host_proxy_->GetCurrentWallTime())
             << ".";
  next_renewal_message_ = msg_stream.str();

  cdm_host_proxy_->SetTimer(timer_delay_ms_, &next_renewal_message_[0]);

  // Use a smaller timer delay at start-up to facilitate testing. Increase the
  // timer delay up to a limit to avoid message spam.
  if (timer_delay_ms_ < kMaxTimerDelayMs)
    timer_delay_ms_ = std::min(2 * timer_delay_ms_, kMaxTimerDelayMs);
}

cdm::Status ClearKeyCdm::DecryptToMediaDecoderBuffer(
    const cdm::InputBuffer_2& encrypted_buffer,
    scoped_refptr<DecoderBuffer>* decrypted_buffer) {
  DCHECK(decrypted_buffer);

  if (!IsSupportedBufferEncryptionScheme(encrypted_buffer.encryption_scheme,
                                         encrypted_buffer.pattern)) {
    return cdm::kDecryptError;
  }

  scoped_refptr<DecoderBuffer> buffer = CopyDecoderBufferFrom(encrypted_buffer);

  // EOS and unencrypted streams can be returned as-is.
  if (buffer->end_of_stream() || !buffer->decrypt_config()) {
    *decrypted_buffer = std::move(buffer);
    return cdm::kSuccess;
  }

  // Callback is called synchronously, so we can use variables on the stack.
  media::Decryptor::Status status = media::Decryptor::kError;
  // The CDM does not care what the stream type is. Pass kVideo
  // for both audio and video decryption.
  cdm_->GetCdmContext()->GetDecryptor()->Decrypt(
      media::Decryptor::kVideo, std::move(buffer),
      base::Bind(&CopyDecryptResults, &status, decrypted_buffer));

  if (status == media::Decryptor::kError)
    return cdm::kDecryptError;

  if (status == media::Decryptor::kNoKey)
    return cdm::kNoKey;

  DCHECK_EQ(status, media::Decryptor::kSuccess);
  return cdm::kSuccess;
}

void ClearKeyCdm::OnPlatformChallengeResponse(
    const cdm::PlatformChallengeResponse& response) {
  DVLOG(1) << __func__;

  if (!is_running_platform_verification_test_) {
    NOTREACHED() << "OnPlatformChallengeResponse() called unexpectedly.";
    return;
  }

  is_running_platform_verification_test_ = false;

  // We are good as long as we get some response back. Ignore the challenge
  // response for now.
  // TODO(xhwang): Also test host challenge here.
  OnUnitTestComplete(true);
}

void ClearKeyCdm::OnQueryOutputProtectionStatus(
    cdm::QueryResult result,
    uint32_t link_mask,
    uint32_t output_protection_mask) {
  DVLOG(1) << __func__;

  if (!is_running_output_protection_test_) {
    NOTREACHED() << "OnQueryOutputProtectionStatus() called unexpectedly.";
    return;
  }

  is_running_output_protection_test_ = false;

// On Chrome OS, status query will fail on Linux Chrome OS build. So we ignore
// the query result. On all other platforms, status query should succeed.
// TODO(xhwang): Improve the check on Chrome OS builds. For example, use
// base::SysInfo::IsRunningOnChromeOS() to differentiate between real Chrome OS
// build and Linux Chrome OS build.
#if !defined(OS_CHROMEOS)
  if (result != cdm::kQuerySucceeded || link_mask != 0) {
    OnUnitTestComplete(false);
    return;
  }
#endif
  OnUnitTestComplete(true);
};

void ClearKeyCdm::OnStorageId(uint32_t version,
                              const uint8_t* storage_id,
                              uint32_t storage_id_size) {
  if (!is_running_storage_id_test_) {
    NOTREACHED() << "OnStorageId() called unexpectedly.";
    return;
  }

  is_running_storage_id_test_ = false;
  DVLOG(1) << __func__ << ": storage_id (hex encoded) = "
           << (storage_id_size ? base::HexEncode(storage_id, storage_id_size)
                               : "<empty>");

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
  // Storage Id is enabled, so something should be returned. It should be the
  // length of a SHA-256 hash (256 bits).
  constexpr uint32_t kExpectedStorageIdSizeInBytes = 256 / 8;
  OnUnitTestComplete(storage_id_size == kExpectedStorageIdSizeInBytes);
#else
  // Storage Id not available, so an empty vector should be returned.
  OnUnitTestComplete(storage_id_size == 0);
#endif
}

void ClearKeyCdm::OnSessionMessage(const std::string& session_id,
                                   CdmMessageType message_type,
                                   const std::vector<uint8_t>& message) {
  DVLOG(1) << __func__ << ": size = " << message.size();

  cdm_host_proxy_->OnSessionMessage(
      session_id.data(), session_id.length(), ConvertMessageType(message_type),
      reinterpret_cast<const char*>(message.data()), message.size());
}

void ClearKeyCdm::OnSessionKeysChange(const std::string& session_id,
                                      bool has_additional_usable_key,
                                      CdmKeysInfo keys_info) {
  DVLOG(1) << __func__ << ": size = " << keys_info.size();

  // Crash if the special key ID "crash" is present.
  const std::vector<uint8_t> kCrashKeyId{'c', 'r', 'a', 's', 'h'};
  for (const auto& key_info : keys_info) {
    if (key_info->key_id == kCrashKeyId)
      CHECK(false) << "Crash on special crash key ID.";
  }

  std::vector<cdm::KeyInformation> keys_vector;
  ConvertCdmKeysInfo(keys_info, &keys_vector);
  cdm_host_proxy_->OnSessionKeysChange(session_id.data(), session_id.length(),
                                       has_additional_usable_key,
                                       keys_vector.data(), keys_vector.size());
}

void ClearKeyCdm::OnSessionClosed(const std::string& session_id) {
  cdm_host_proxy_->OnSessionClosed(session_id.data(), session_id.length());
}

void ClearKeyCdm::OnSessionExpirationUpdate(const std::string& session_id,
                                            base::Time new_expiry_time) {
  DVLOG(1) << __func__ << ": expiry_time = " << new_expiry_time;
  cdm_host_proxy_->OnExpirationChange(session_id.data(), session_id.length(),
                                      new_expiry_time.ToDoubleT());
}

void ClearKeyCdm::OnSessionCreated(uint32_t promise_id,
                                   const std::string& session_id) {
  // Save the latest session ID for renewal and file IO test messages.
  last_session_id_ = session_id;

  cdm_host_proxy_->OnResolveNewSessionPromise(promise_id, session_id.data(),
                                              session_id.length());
}

void ClearKeyCdm::OnPromiseResolved(uint32_t promise_id) {
  cdm_host_proxy_->OnResolvePromise(promise_id);
}

void ClearKeyCdm::OnPromiseFailed(uint32_t promise_id,
                                  CdmPromise::Exception exception_code,
                                  uint32_t system_code,
                                  const std::string& error_message) {
  DVLOG(1) << __func__ << ": error = " << error_message;
  cdm_host_proxy_->OnRejectPromise(promise_id, ConvertException(exception_code),
                                   system_code, error_message.data(),
                                   error_message.length());
}

void ClearKeyCdm::OnUnitTestComplete(bool success) {
  std::string message = GetUnitTestResultMessage(success);
  cdm_host_proxy_->OnSessionMessage(
      last_session_id_.data(), last_session_id_.length(), cdm::kLicenseRequest,
      message.data(), message.length());
}

void ClearKeyCdm::StartFileIOTest() {
  file_io_test_runner_.reset(new FileIOTestRunner(base::Bind(
      &CdmHostProxy::CreateFileIO, base::Unretained(cdm_host_proxy_.get()))));
  file_io_test_runner_->RunAllTests(
      base::Bind(&ClearKeyCdm::OnFileIOTestComplete, base::Unretained(this)));
}

void ClearKeyCdm::OnFileIOTestComplete(bool success) {
  DVLOG(1) << __func__ << ": " << success;
  OnUnitTestComplete(success);
  file_io_test_runner_.reset();
}

void ClearKeyCdm::StartOutputProtectionTest() {
  DVLOG(1) << __func__;
  is_running_output_protection_test_ = true;
  cdm_host_proxy_->QueryOutputProtectionStatus();
}

void ClearKeyCdm::StartPlatformVerificationTest() {
  DVLOG(1) << __func__;
  is_running_platform_verification_test_ = true;

  std::string service_id = "test_service_id";
  std::string challenge = "test_challenge";

  cdm_host_proxy_->SendPlatformChallenge(service_id.data(), service_id.size(),
                                         challenge.data(), challenge.size());
}

void ClearKeyCdm::ReportVerifyCdmHostTestResult() {
  // VerifyCdmHost() should have already been called and test result stored
  // in |g_verify_host_files_result|.
  OnUnitTestComplete(g_verify_host_files_result);
}

void ClearKeyCdm::StartStorageIdTest() {
  DVLOG(1) << __func__;
  is_running_storage_id_test_ = true;

  // Request the latest available version.
  cdm_host_proxy_->RequestStorageId(0);
}

void ClearKeyCdm::StartCdmProxyTest() {
  DVLOG(1) << __func__;
  DCHECK(!cdm_proxy_test_);

  cdm_proxy_test_.reset(new CdmProxyTest(cdm_host_proxy_.get()));
  cdm_proxy_test_->Run(base::BindOnce(&ClearKeyCdm::OnCdmProxyTestComplete,
                                      base::Unretained(this)));
}

void ClearKeyCdm::OnCdmProxyTestComplete(bool success) {
  DVLOG(1) << __func__;
  DCHECK(cdm_proxy_test_);

  cdm_proxy_test_.reset();
  has_cdm_proxy_test_passed_ = success;

  // Ignore test result here. It will be reported in ReportCdmProxyTestResult().
  cdm_host_proxy_->OnInitialized(true);
}

void ClearKeyCdm::ReportCdmProxyTestResult() {
  // StartCdmProxyTest() should have already been called and finished.
  OnUnitTestComplete(has_cdm_proxy_test_passed_);
}

}  // namespace media
