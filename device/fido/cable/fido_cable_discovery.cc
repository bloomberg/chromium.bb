// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_discovery.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/cable/fido_ble_uuids.h"
#include "device/fido/cable/fido_cable_handshake_handler.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

namespace device {

namespace {

// Construct advertisement data with different formats depending on client's
// operating system. Ideally, we advertise EIDs as part of Service Data, but
// this isn't available on all platforms. On Windows we use Manufacturer Data
// instead, and on Mac our only option is to advertise an additional service
// with the EID as its UUID.
std::unique_ptr<BluetoothAdvertisement::Data> ConstructAdvertisementData(
    base::span<const uint8_t, kCableEphemeralIdSize> client_eid) {
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::AdvertisementType::ADVERTISEMENT_TYPE_BROADCAST);

#if defined(OS_MACOSX)
  auto list = std::make_unique<BluetoothAdvertisement::UUIDList>();
  list->emplace_back(kCableAdvertisementUUID16);
  list->emplace_back(fido_parsing_utils::ConvertBytesToUuid(client_eid));
  advertisement_data->set_service_uuids(std::move(list));

#elif defined(OS_WIN)
  // References:
  // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  // go/google-ble-manufacturer-data-format
  static constexpr uint16_t kGoogleManufacturerId = 0x00E0;
  static constexpr uint8_t kCableGoogleManufacturerDataType = 0x15;

  // Reference:
  // https://github.com/arnar/fido-2-specs/blob/fido-client-to-authenticator-protocol.bs#L4314
  static constexpr uint8_t kCableFlags = 0x20;

  static constexpr uint8_t kCableGoogleManufacturerDataLength =
      3u + kCableEphemeralIdSize;
  std::array<uint8_t, 4> kCableGoogleManufacturerDataHeader = {
      kCableGoogleManufacturerDataLength, kCableGoogleManufacturerDataType,
      kCableFlags, /*version=*/1};

  auto manufacturer_data =
      std::make_unique<BluetoothAdvertisement::ManufacturerData>();
  std::vector<uint8_t> manufacturer_data_value;
  fido_parsing_utils::Append(&manufacturer_data_value,
                             kCableGoogleManufacturerDataHeader);
  fido_parsing_utils::Append(&manufacturer_data_value, client_eid);
  manufacturer_data->emplace(kGoogleManufacturerId,
                             std::move(manufacturer_data_value));
  advertisement_data->set_manufacturer_data(std::move(manufacturer_data));

#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Reference:
  // https://github.com/arnar/fido-2-specs/blob/fido-client-to-authenticator-protocol.bs#L4314
  static constexpr uint8_t kCableFlags = 0x20;

  // Service data for ChromeOS and Linux is 1 byte corresponding to Cable flags,
  // followed by 1 byte corresponding to Cable version number, followed by 16
  // bytes corresponding to client EID.
  auto service_data = std::make_unique<BluetoothAdvertisement::ServiceData>();
  std::vector<uint8_t> service_data_value(18, 0);
  // Since the remainder of this service data field is a Cable EID, set the 5th
  // bit of the flag byte.
  service_data_value[0] = kCableFlags;
  service_data_value[1] = 1 /* version */;
  std::copy(client_eid.begin(), client_eid.end(),
            service_data_value.begin() + 2);
  service_data->emplace(kCableAdvertisementUUID128,
                        std::move(service_data_value));
  advertisement_data->set_service_data(std::move(service_data));
#endif

  return advertisement_data;
}

enum class QRValue : uint8_t {
  QR_SECRET = 0,
  IDENTITY_KEY_SEED = 1,
};

void DeriveQRValue(base::span<const uint8_t, 32> qr_generator_key,
                   const int64_t tick,
                   QRValue type,
                   base::span<uint8_t> out) {
  uint8_t hkdf_input[sizeof(uint64_t) + 1];
  memcpy(hkdf_input, &tick, sizeof(uint64_t));
  hkdf_input[sizeof(uint64_t)] = base::strict_cast<uint8_t>(type);

  bool ok = HKDF(out.data(), out.size(), EVP_sha256(), qr_generator_key.data(),
                 qr_generator_key.size(),
                 /*salt=*/nullptr, 0, hkdf_input, sizeof(hkdf_input));
  DCHECK(ok);
}

}  // namespace

// CableDiscoveryData -------------------------------------

CableDiscoveryData::CableDiscoveryData() = default;

CableDiscoveryData::CableDiscoveryData(
    CableDiscoveryData::Version version,
    const CableEidArray& client_eid,
    const CableEidArray& authenticator_eid,
    const CableSessionPreKeyArray& session_pre_key)
    : version(version) {
  CHECK_EQ(Version::V1, version);
  v1.emplace();
  v1->client_eid = client_eid;
  v1->authenticator_eid = authenticator_eid;
  v1->session_pre_key = session_pre_key;
}

CableDiscoveryData::CableDiscoveryData(
    base::span<const uint8_t, kCableQRSecretSize> qr_secret,
    base::span<const uint8_t, kCableIdentityKeySeedSize> identity_key_seed) {
  InitFromQRSecret(qr_secret);
  v2->local_identity_seed = fido_parsing_utils::Materialize(identity_key_seed);
}

// static
base::Optional<CableDiscoveryData> CableDiscoveryData::FromQRData(
    base::span<const uint8_t,
               kCableCompressedPublicKeySize + kCableQRSecretSize> qr_data) {
  auto qr_secret = qr_data.subspan(kCableCompressedPublicKeySize);
  CableDiscoveryData discovery_data;
  discovery_data.InitFromQRSecret(base::span<const uint8_t, kCableQRSecretSize>(
      qr_secret.data(), qr_secret.size()));

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), qr_data.data(),
                          kCableCompressedPublicKeySize, /*ctx=*/nullptr)) {
    return base::nullopt;
  }
  CableAuthenticatorIdentityKey& identity_key =
      discovery_data.v2->peer_identity.emplace();
  CHECK_EQ(identity_key.size(),
           EC_POINT_point2oct(
               p256.get(), point.get(), POINT_CONVERSION_UNCOMPRESSED,
               identity_key.data(), identity_key.size(), /*ctx=*/nullptr));

  return discovery_data;
}

CableDiscoveryData::CableDiscoveryData(const CableDiscoveryData& data) =
    default;

CableDiscoveryData& CableDiscoveryData::operator=(
    const CableDiscoveryData& other) = default;

CableDiscoveryData::~CableDiscoveryData() = default;

bool CableDiscoveryData::operator==(const CableDiscoveryData& other) const {
  if (version != other.version) {
    return false;
  }

  switch (version) {
    case CableDiscoveryData::Version::V1:
      return v1->client_eid == other.v1->client_eid &&
             v1->authenticator_eid == other.v1->authenticator_eid &&
             v1->session_pre_key == other.v1->session_pre_key;

    case CableDiscoveryData::Version::V2:
      return v2->eid_gen_key == other.v2->eid_gen_key &&
             v2->psk_gen_key == other.v2->psk_gen_key &&
             v2->peer_identity == other.v2->peer_identity &&
             v2->peer_name == other.v2->peer_name;

    case CableDiscoveryData::Version::INVALID:
      CHECK(false);
      return false;
  }
}

base::Optional<CableNonce> CableDiscoveryData::Match(
    const CableEidArray& eid) const {
  switch (version) {
    case Version::V1: {
      if (eid != v1->authenticator_eid) {
        return base::nullopt;
      }

      // The nonce is the first eight bytes of the EID.
      CableNonce nonce;
      const bool ok =
          fido_parsing_utils::ExtractArray(v1->client_eid, 0, &nonce);
      DCHECK(ok);
      return nonce;
    }

    case Version::V2: {
      // Attempt to decrypt the EID with the EID generator key and check whether
      // it has a valid structure.
      AES_KEY key;
      CHECK(AES_set_decrypt_key(v2->eid_gen_key.data(),
                                /*bits=*/8 * v2->eid_gen_key.size(),
                                &key) == 0);
      static_assert(kCableEphemeralIdSize == AES_BLOCK_SIZE,
                    "EIDs are not AES blocks");
      CableEidArray decrypted;
      AES_decrypt(/*in=*/eid.data(), /*out=*/decrypted.data(), &key);
      const uint8_t kZeroTrailer[8] = {0};
      static_assert(8 + sizeof(kZeroTrailer) ==
                        std::tuple_size<decltype(decrypted)>::value,
                    "Trailer is wrong size");
      if (CRYPTO_memcmp(kZeroTrailer, decrypted.data() + 8,
                        sizeof(kZeroTrailer)) != 0) {
        return base::nullopt;
      }

      CableNonce nonce;
      static_assert(
          sizeof(nonce) <= std::tuple_size<decltype(decrypted)>::value,
          "nonce too large");
      memcpy(nonce.data(), decrypted.data(), sizeof(nonce));
      return nonce;
    }

    case Version::INVALID:
      DCHECK(false);
      return base::nullopt;
  }
}

// static
QRGeneratorKey CableDiscoveryData::NewQRKey() {
  QRGeneratorKey key;
  crypto::RandBytes(key.data(), key.size());
  return key;
}

// static
int64_t CableDiscoveryData::CurrentTimeTick() {
  // The ticks are currently 256ms.
  return base::TimeTicks::Now().since_origin().InMilliseconds() >> 8;
}

// static
std::array<uint8_t, kCableQRSecretSize> CableDiscoveryData::DeriveQRSecret(
    base::span<const uint8_t, 32> qr_generator_key,
    const int64_t tick) {
  std::array<uint8_t, kCableQRSecretSize> ret;
  DeriveQRValue(qr_generator_key, tick, QRValue::QR_SECRET, ret);
  return ret;
}

// static
CableIdentityKeySeed CableDiscoveryData::DeriveIdentityKeySeed(
    base::span<const uint8_t, 32> qr_generator_key,
    const int64_t tick) {
  std::array<uint8_t, kCableIdentityKeySeedSize> ret;
  DeriveQRValue(qr_generator_key, tick, QRValue::IDENTITY_KEY_SEED, ret);
  return ret;
}

// static
CableQRData CableDiscoveryData::DeriveQRData(
    base::span<const uint8_t, 32> qr_generator_key,
    const int64_t tick) {
  auto identity_key_seed = DeriveIdentityKeySeed(qr_generator_key, tick);
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> identity_key(EC_KEY_derive_from_secret(
      p256.get(), identity_key_seed.data(), identity_key_seed.size()));
  const EC_POINT* public_key = EC_KEY_get0_public_key(identity_key.get());
  CableQRData qr_data;
  static_assert(
      qr_data.size() == kCableCompressedPublicKeySize + kCableQRSecretSize,
      "this code needs to be updated");
  CHECK_EQ(kCableCompressedPublicKeySize,
           EC_POINT_point2oct(p256.get(), public_key,
                              POINT_CONVERSION_COMPRESSED, qr_data.data(),
                              kCableCompressedPublicKeySize, /*ctx=*/nullptr));

  auto qr_secret = CableDiscoveryData::DeriveQRSecret(qr_generator_key, tick);
  memcpy(&qr_data.data()[kCableCompressedPublicKeySize], qr_secret.data(),
         qr_secret.size());

  return qr_data;
}

CableDiscoveryData::V2Data::V2Data() = default;
CableDiscoveryData::V2Data::V2Data(const V2Data&) = default;
CableDiscoveryData::V2Data::~V2Data() = default;

void CableDiscoveryData::InitFromQRSecret(
    base::span<const uint8_t, kCableQRSecretSize> qr_secret) {
  version = Version::V2;
  v2.emplace();

  static const char kEIDGen[] = "caBLE QR to EID generator key";
  bool ok =
      HKDF(v2->eid_gen_key.data(), v2->eid_gen_key.size(), EVP_sha256(),
           qr_secret.data(), qr_secret.size(), /*salt=*/nullptr, 0,
           reinterpret_cast<const uint8_t*>(kEIDGen), sizeof(kEIDGen) - 1);
  DCHECK(ok);

  static const char kPSKGen[] = "caBLE QR to PSK generator key";
  ok = HKDF(v2->psk_gen_key.data(), v2->psk_gen_key.size(), EVP_sha256(),
            qr_secret.data(), qr_secret.size(), /*salt=*/nullptr, 0,
            reinterpret_cast<const uint8_t*>(kPSKGen), sizeof(kPSKGen) - 1);
  DCHECK(ok);
}

// FidoCableDiscovery::CableV1DiscoveryEvent  ---------------------------------

// CableV1DiscoveryEvent enumerates several things that can occur during a caBLE
// v1 discovery. Do not change assigned values since they are used in
// histograms, only append new values. Keep synced with enums.xml.
enum class FidoCableDiscovery::CableV1DiscoveryEvent : int {
  kStarted = 0,
  kAdapterPresent = 1,
  kAdapterAlreadyPowered = 2,
  kAdapterAutomaticallyPowered = 3,
  kAdapterManuallyPowered = 4,
  kAdapterPoweredOff = 5,
  kScanningStarted = 6,
  kAdvertisementRegistered = 7,
  kFirstCableDeviceFound = 8,
  kFirstCableDeviceGATTConnected = 9,
  kFirstCableHandshakeSucceeded = 10,
  kFirstCableDeviceTimeout = 11,
  kMaxValue = kFirstCableDeviceTimeout,
};

// FidoCableDiscovery::Result -------------------------------------------------

FidoCableDiscovery::Result::Result() = default;

FidoCableDiscovery::Result::Result(const CableDiscoveryData& in_discovery_data,
                                   const CableNonce& in_nonce,
                                   const CableEidArray& in_eid,
                                   base::Optional<int> in_ticks_back)
    : discovery_data(in_discovery_data),
      nonce(in_nonce),
      eid(in_eid),
      ticks_back(in_ticks_back) {}

FidoCableDiscovery::Result::Result(const Result& other) = default;

FidoCableDiscovery::Result::~Result() = default;

// FidoCableDiscovery::ObservedDeviceData -------------------------------------

FidoCableDiscovery::ObservedDeviceData::ObservedDeviceData() = default;
FidoCableDiscovery::ObservedDeviceData::~ObservedDeviceData() = default;

// FidoCableDiscovery ---------------------------------------------------------

FidoCableDiscovery::FidoCableDiscovery(
    std::vector<CableDiscoveryData> discovery_data,
    base::Optional<QRGeneratorKey> qr_generator_key,
    base::Optional<
        base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>>
        pairing_callback)
    : FidoDeviceDiscovery(
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy),
      discovery_data_(std::move(discovery_data)),
      qr_generator_key_(std::move(qr_generator_key)),
      pairing_callback_(std::move(pairing_callback)) {
// Windows currently does not support multiple EIDs, thus we ignore any extra
// discovery data.
// TODO(https://crbug.com/837088): Add support for multiple EIDs on Windows.
#if defined(OS_WIN)
  if (discovery_data_.size() > 1u) {
    FIDO_LOG(ERROR) << "discovery_data_.size()=" << discovery_data_.size()
                    << ", trimming to 1.";
    discovery_data_.erase(discovery_data_.begin() + 1, discovery_data_.end());
  }
#endif
  for (const CableDiscoveryData& data : discovery_data_) {
    if (data.version != CableDiscoveryData::Version::V1) {
      continue;
    }
    has_v1_discovery_data_ = true;
    RecordCableV1DiscoveryEventOnce(CableV1DiscoveryEvent::kStarted);
    break;
  }
}

FidoCableDiscovery::~FidoCableDiscovery() {
  // Work around dangling advertisement references. (crbug/846522)
  for (auto advertisement : advertisements_) {
    advertisement.second->Unregister(base::DoNothing(), base::DoNothing());
  }

  if (adapter_)
    adapter_->RemoveObserver(this);
}

base::Optional<std::unique_ptr<FidoCableHandshakeHandler>>
FidoCableDiscovery::CreateHandshakeHandler(
    FidoCableDevice* device,
    const CableDiscoveryData& discovery_data,
    const CableNonce& nonce,
    const CableEidArray& eid) {
  std::unique_ptr<FidoCableHandshakeHandler> handler;
  switch (discovery_data.version) {
    case CableDiscoveryData::Version::V1: {
      // Nonce is embedded as first 8 bytes of client EID.
      std::array<uint8_t, 8> nonce;
      const bool ok = fido_parsing_utils::ExtractArray(
          discovery_data.v1->client_eid, 0, &nonce);
      DCHECK(ok);

      handler.reset(new FidoCableV1HandshakeHandler(
          device, nonce, discovery_data.v1->session_pre_key));
      break;
    }

    case CableDiscoveryData::Version::V2: {
      if (!base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
        return base::nullopt;
      }
      if (!pairing_callback_) {
        FIDO_LOG(DEBUG) << "Discarding caBLE v2 handshake because of missing "
                           "pairing callback";
        return base::nullopt;
      }

      handler.reset(new FidoCableV2HandshakeHandler(
          device, discovery_data.v2->psk_gen_key, nonce, eid,
          discovery_data.v2->peer_identity,
          discovery_data.v2->local_identity_seed, *pairing_callback_));
      break;
    }

    case CableDiscoveryData::Version::INVALID:
      CHECK(false);
      return base::nullopt;
  }

  return handler;
}

// static
const BluetoothUUID& FidoCableDiscovery::CableAdvertisementUUID() {
  static const base::NoDestructor<BluetoothUUID> service_uuid(
      kCableAdvertisementUUID128);
  return *service_uuid;
}

// static
bool FidoCableDiscovery::IsCableDevice(const BluetoothDevice* device) {
  const auto& uuid = CableAdvertisementUUID();
  return base::Contains(device->GetServiceData(), uuid) ||
         base::Contains(device->GetUUIDs(), uuid);
}

void FidoCableDiscovery::OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    FIDO_LOG(DEBUG) << "No BLE adapter present";
    NotifyDiscoveryStarted(false);
    return;
  }

  if (has_v1_discovery_data_) {
    RecordCableV1DiscoveryEventOnce(CableV1DiscoveryEvent::kAdapterPresent);
    if (adapter->IsPowered()) {
      RecordCableV1DiscoveryEventOnce(
          CableV1DiscoveryEvent::kAdapterAlreadyPowered);
    }
  }

  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  FIDO_LOG(DEBUG) << "BLE adapter address " << adapter_->GetAddress();

  adapter_->AddObserver(this);
  if (adapter_->IsPowered()) {
    OnSetPowered();
  }

  // FidoCableDiscovery blocks its transport availability callback on the
  // DiscoveryStarted() calls of all instantiated discoveries. Hence, this call
  // must not be put behind the BLE adapter getting powered on (which is
  // dependent on the UI), or else the UI and this discovery will wait on each
  // other indefinitely (see crbug.com/1018416).
  NotifyDiscoveryStarted(true);
}

void FidoCableDiscovery::OnSetPowered() {
  DCHECK(adapter());
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FidoCableDiscovery::StartCableDiscovery,
                                weak_factory_.GetWeakPtr()));
}

void FidoCableDiscovery::SetDiscoverySession(
    std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
  discovery_session_ = std::move(discovery_session);
}

void FidoCableDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (!IsCableDevice(device))
    return;

  CableDeviceFound(adapter, device);
}

void FidoCableDiscovery::DeviceChanged(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (!IsCableDevice(device))
    return;

  CableDeviceFound(adapter, device);
}

void FidoCableDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (IsCableDevice(device) && GetCableDiscoveryData(device)) {
    const auto& device_address = device->GetAddress();
    FIDO_LOG(DEBUG) << "caBLE device removed: " << device_address;
    RemoveDevice(FidoCableDevice::GetIdForAddress(device_address));
  }
}

void FidoCableDiscovery::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                               bool powered) {
  if (has_v1_discovery_data_) {
    RecordCableV1DiscoveryEventOnce(
        powered ? (adapter->CanPower()
                       ? CableV1DiscoveryEvent::kAdapterAutomaticallyPowered
                       : CableV1DiscoveryEvent::kAdapterManuallyPowered)
                : CableV1DiscoveryEvent::kAdapterPoweredOff);
  }

  if (!powered) {
    // In order to prevent duplicate client EIDs from being advertised when
    // BluetoothAdapter is powered back on, unregister all existing client
    // EIDs.
    StopAdvertisements(base::DoNothing());
    return;
  }

#if defined(OS_WIN)
  // On Windows, the power-on event appears to race against initialization of
  // the adapter, such that one of the WinRT API calls inside
  // BluetoothAdapter::StartDiscoverySessionWithFilter() can fail with "Device
  // not ready for use". So wait for things to actually be ready.
  // TODO(crbug/1046140): Remove this delay once the Bluetooth layer handles
  // the spurious failure.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FidoCableDiscovery::StartCableDiscovery,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(500));
#else
  StartCableDiscovery();
#endif  // defined(OS_WIN)
}

void FidoCableDiscovery::FidoCableDeviceConnected(FidoCableDevice* device,
                                                  bool success) {
  if (!success || !IsObservedV1Device(device->GetAddress())) {
    return;
  }
  RecordCableV1DiscoveryEventOnce(
      CableV1DiscoveryEvent::kFirstCableDeviceGATTConnected);
}

void FidoCableDiscovery::FidoCableDeviceTimeout(FidoCableDevice* device) {
  if (!IsObservedV1Device(device->GetAddress())) {
    return;
  }
  RecordCableV1DiscoveryEventOnce(
      CableV1DiscoveryEvent::kFirstCableDeviceTimeout);
}

void FidoCableDiscovery::StartCableDiscovery() {
  adapter()->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(
          BluetoothTransport::BLUETOOTH_TRANSPORT_LE),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySession,
                         weak_factory_.GetWeakPtr())),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySessionError,
                         weak_factory_.GetWeakPtr())));
}

void FidoCableDiscovery::OnStartDiscoverySession(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  FIDO_LOG(DEBUG) << "Discovery session started.";
  if (has_v1_discovery_data_) {
    RecordCableV1DiscoveryEventOnce(CableV1DiscoveryEvent::kScanningStarted);
  }
  SetDiscoverySession(std::move(session));
  // Advertising is delayed by 500ms to ensure that any UI has a chance to
  // appear as we don't want to start broadcasting without the user being
  // aware.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FidoCableDiscovery::StartAdvertisement,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(500));
}

void FidoCableDiscovery::OnStartDiscoverySessionError() {
  FIDO_LOG(ERROR) << "Failed to start caBLE discovery";
}

void FidoCableDiscovery::StartAdvertisement() {
  DCHECK(adapter());
  bool advertisements_pending = false;
  for (const auto& data : discovery_data_) {
    if (data.version != CableDiscoveryData::Version::V1) {
      continue;
    }

    if (!advertisements_pending) {
      FIDO_LOG(DEBUG) << "Starting to advertise clientEIDs.";
      advertisements_pending = true;
    }
    adapter()->RegisterAdvertisement(
        ConstructAdvertisementData(data.v1->client_eid),
        base::AdaptCallbackForRepeating(
            base::BindOnce(&FidoCableDiscovery::OnAdvertisementRegistered,
                           weak_factory_.GetWeakPtr(), data.v1->client_eid)),
        base::BindRepeating([](BluetoothAdvertisement::ErrorCode error_code) {
          FIDO_LOG(ERROR) << "Failed to register advertisement: " << error_code;
        }));
  }
}

void FidoCableDiscovery::StopAdvertisements(base::OnceClosure callback) {
  // Destructing a BluetoothAdvertisement invokes its Unregister() method, but
  // there may be references to the advertisement outside this
  // FidoCableDiscovery (see e.g. crbug/846522). Hence, merely clearing
  // |advertisements_| is not sufficient; we need to manually invoke
  // Unregister() for every advertisement in order to stop them. On the other
  // hand, |advertisements_| must not be cleared before the Unregister()
  // callbacks return either, in case we do hold the only reference to a
  // BluetoothAdvertisement.
  FIDO_LOG(DEBUG) << "Stopping " << advertisements_.size()
                  << " caBLE advertisements";
  auto barrier_closure = base::BarrierClosure(
      advertisements_.size(),
      base::BindOnce(&FidoCableDiscovery::OnAdvertisementsStopped,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  auto error_closure = base::BindRepeating(
      [](base::RepeatingClosure cb, BluetoothAdvertisement::ErrorCode code) {
        FIDO_LOG(ERROR) << "BluetoothAdvertisement::Unregister() failed: "
                        << code;
        cb.Run();
      },
      barrier_closure);
  for (auto advertisement : advertisements_) {
    advertisement.second->Unregister(barrier_closure, error_closure);
  }
}

void FidoCableDiscovery::OnAdvertisementsStopped(base::OnceClosure callback) {
  FIDO_LOG(DEBUG) << "Advertisements stopped";
  advertisements_.clear();
  std::move(callback).Run();
}

void FidoCableDiscovery::OnAdvertisementRegistered(
    const CableEidArray& client_eid,
    scoped_refptr<BluetoothAdvertisement> advertisement) {
  FIDO_LOG(DEBUG) << "Advertisement registered";
  RecordCableV1DiscoveryEventOnce(
      CableV1DiscoveryEvent::kAdvertisementRegistered);
  advertisements_.emplace(client_eid, std::move(advertisement));
}

void FidoCableDiscovery::CableDeviceFound(BluetoothAdapter* adapter,
                                          BluetoothDevice* device) {
  const std::string device_address = device->GetAddress();
  if (base::Contains(active_devices_, device_address)) {
    return;
  }

  base::Optional<Result> result = GetCableDiscoveryData(device);
  if (!result || base::Contains(active_authenticator_eids_, result->eid)) {
    return;
  }

  FIDO_LOG(EVENT) << "Found new caBLE device.";
  if (result->discovery_data.version == CableDiscoveryData::Version::V1) {
    RecordCableV1DiscoveryEventOnce(
        CableV1DiscoveryEvent::kFirstCableDeviceFound);
  }
  active_devices_.insert(device_address);
  active_authenticator_eids_.insert(result->eid);

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  // Speed up GATT service discovery on ChromeOS/BlueZ.
  // SetConnectionLatency() is NOTIMPLEMENTED() on other platforms.
  if (base::FeatureList::IsEnabled(device::kWebAuthCableLowLatency)) {
    device->SetConnectionLatency(BluetoothDevice::CONNECTION_LATENCY_LOW,
                                 base::DoNothing(), base::BindRepeating([]() {
                                   FIDO_LOG(ERROR)
                                       << "SetConnectionLatency() failed";
                                 }));
  }
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

  auto cable_device =
      std::make_unique<FidoCableDevice>(adapter, device_address);
  cable_device->set_observer(this);

  base::Optional<std::unique_ptr<FidoCableHandshakeHandler>> handshake_handler =
      CreateHandshakeHandler(cable_device.get(), result->discovery_data,
                             result->nonce, result->eid);
  if (!handshake_handler) {
    return;
  }
  auto* const handshake_handler_ptr = handshake_handler->get();
  active_handshakes_.emplace_back(std::move(cable_device),
                                  std::move(*handshake_handler));

  StopAdvertisements(
      base::BindOnce(&FidoCableDiscovery::ConductEncryptionHandshake,
                     weak_factory_.GetWeakPtr(), handshake_handler_ptr,
                     result->discovery_data.version));
}

void FidoCableDiscovery::ConductEncryptionHandshake(
    FidoCableHandshakeHandler* handshake_handler,
    CableDiscoveryData::Version cable_version) {
  handshake_handler->InitiateCableHandshake(base::BindOnce(
      &FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage,
      weak_factory_.GetWeakPtr(), cable_version, handshake_handler));
}

void FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage(
    CableDiscoveryData::Version cable_version,
    FidoCableHandshakeHandler* handshake_handler,
    base::Optional<std::vector<uint8_t>> handshake_response) {
  const bool ok = handshake_response.has_value() &&
                  handshake_handler->ValidateAuthenticatorHandshakeMessage(
                      *handshake_response);

  bool found = false;
  for (auto it = active_handshakes_.begin(); it != active_handshakes_.end();
       it++) {
    if (it->second.get() != handshake_handler) {
      continue;
    }

    found = true;
    if (ok) {
      AddDevice(std::move(it->first));
    }
    active_handshakes_.erase(it);
    break;
  }
  DCHECK(found);

  if (ok) {
    FIDO_LOG(DEBUG) << "Authenticator handshake validated";
    if (cable_version == CableDiscoveryData::Version::V1) {
      RecordCableV1DiscoveryEventOnce(
          CableV1DiscoveryEvent::kFirstCableHandshakeSucceeded);
    }
  } else {
    FIDO_LOG(DEBUG) << "Authenticator handshake invalid";
  }
}

base::Optional<FidoCableDiscovery::Result>
FidoCableDiscovery::GetCableDiscoveryData(const BluetoothDevice* device) const {
  base::Optional<CableEidArray> maybe_eid_from_service_data =
      MaybeGetEidFromServiceData(device);
  std::vector<CableEidArray> uuids = GetUUIDs(device);

  const std::string address = device->GetAddress();
  const auto it = observed_devices_.find(address);
  const bool known = it != observed_devices_.end();
  if (known) {
    std::unique_ptr<ObservedDeviceData>& data = it->second;
    if (maybe_eid_from_service_data == data->service_data &&
        uuids == data->uuids) {
      // Duplicate data. Ignore.
      return base::nullopt;
    }
  }

  // New or updated device information.
  if (known) {
    FIDO_LOG(DEBUG) << "Updated information for caBLE device " << address
                    << ":";
  } else {
    FIDO_LOG(DEBUG) << "New caBLE device " << address << ":";
  }

  base::Optional<FidoCableDiscovery::Result> result;
  if (maybe_eid_from_service_data.has_value()) {
    result =
        GetCableDiscoveryDataFromAuthenticatorEid(*maybe_eid_from_service_data);
    FIDO_LOG(DEBUG) << "  Service data: "
                    << ResultDebugString(*maybe_eid_from_service_data, result);

  } else {
    FIDO_LOG(DEBUG) << "  Service data: <none>";
  }

  if (!uuids.empty()) {
    FIDO_LOG(DEBUG) << "  UUIDs:";
    for (const auto& uuid : uuids) {
      auto discovery_data = GetCableDiscoveryDataFromAuthenticatorEid(uuid);
      FIDO_LOG(DEBUG) << "    " << ResultDebugString(uuid, discovery_data);
      if (!result && discovery_data) {
        result = discovery_data;
      }
    }
  }

  auto observed_data = std::make_unique<ObservedDeviceData>();
  observed_data->service_data = maybe_eid_from_service_data;
  observed_data->uuids = uuids;
  if (result) {
    observed_data->maybe_discovery_data = result->discovery_data;
  }
  observed_devices_.insert_or_assign(address, std::move(observed_data));

  return result;
}

// static
base::Optional<CableEidArray> FidoCableDiscovery::MaybeGetEidFromServiceData(
    const BluetoothDevice* device) {
  const auto* service_data =
      device->GetServiceDataForUUID(CableAdvertisementUUID());
  if (!service_data) {
    return base::nullopt;
  }

  // Received service data from authenticator must have a flag that signals that
  // the service data includes Cable EID.
  if (service_data->empty() || !(service_data->at(0) >> 5 & 1u))
    return base::nullopt;

  CableEidArray received_authenticator_eid;
  bool extract_success = fido_parsing_utils::ExtractArray(
      *service_data, 2, &received_authenticator_eid);
  if (!extract_success)
    return base::nullopt;
  return received_authenticator_eid;
}

// static
std::vector<CableEidArray> FidoCableDiscovery::GetUUIDs(
    const BluetoothDevice* device) {
  std::vector<CableEidArray> ret;

  const auto service_uuids = device->GetUUIDs();
  for (const auto& uuid : service_uuids) {
    std::vector<uint8_t> uuid_binary = uuid.GetBytes();
    CableEidArray authenticator_eid;
    DCHECK_EQ(authenticator_eid.size(), uuid_binary.size());
    memcpy(authenticator_eid.data(), uuid_binary.data(),
           std::min(uuid_binary.size(), authenticator_eid.size()));

    ret.emplace_back(std::move(authenticator_eid));
  }

  return ret;
}

base::Optional<FidoCableDiscovery::Result>
FidoCableDiscovery::GetCableDiscoveryDataFromAuthenticatorEid(
    CableEidArray authenticator_eid) const {
  for (const auto& candidate : discovery_data_) {
    auto maybe_nonce = candidate.Match(authenticator_eid);
    if (maybe_nonce) {
      return Result(candidate, *maybe_nonce, authenticator_eid, base::nullopt);
    }
  }

  if (qr_generator_key_) {
    // Attempt to match |authenticator_eid| as the result of scanning a QR code.
    const int64_t current_tick = CableDiscoveryData::CurrentTimeTick();
    // kNumPreviousTicks is the number of previous ticks that will be accepted
    // as valid. Ticks are currently 256ms so the value of sixteen translates to
    // about four seconds.
    constexpr int kNumPreviousTicks = 16;

    for (int i = 0; i < kNumPreviousTicks; i++) {
      auto qr_secret = CableDiscoveryData::DeriveQRSecret(*qr_generator_key_,
                                                          current_tick - i);
      auto identity_key_seed = CableDiscoveryData::DeriveIdentityKeySeed(
          *qr_generator_key_, current_tick - i);
      CableDiscoveryData candidate(qr_secret, identity_key_seed);
      auto maybe_nonce = candidate.Match(authenticator_eid);
      if (maybe_nonce) {
        return Result(candidate, *maybe_nonce, authenticator_eid, i);
      }
    }

    if (base::Contains(noted_obsolete_eids_, authenticator_eid)) {
      std::array<uint8_t, kCableIdentityKeySeedSize> dummy_seed;
      for (int i = kNumPreviousTicks; i < 2 * kNumPreviousTicks; i++) {
        auto qr_secret = CableDiscoveryData::DeriveQRSecret(*qr_generator_key_,
                                                            current_tick - i);
        CableDiscoveryData candidate(qr_secret, dummy_seed);
        if (candidate.Match(authenticator_eid)) {
          noted_obsolete_eids_.insert(authenticator_eid);
          FIDO_LOG(DEBUG)
              << "(EID " << base::HexEncode(authenticator_eid) << " is " << i
              << " ticks old and would be valid but for the cutoff)";
          break;
        }
      }
    }
  }

  return base::nullopt;
}

bool FidoCableDiscovery::IsObservedV1Device(const std::string& address) const {
  const auto it = observed_devices_.find(address);
  return it != observed_devices_.end() && it->second->maybe_discovery_data &&
         it->second->maybe_discovery_data->version ==
             CableDiscoveryData::Version::V1;
}

void FidoCableDiscovery::RecordCableV1DiscoveryEventOnce(
    CableV1DiscoveryEvent event) {
  DCHECK(has_v1_discovery_data_);
  if (base::Contains(recorded_events_, event)) {
    return;
  }
  recorded_events_.insert(event);
  base::UmaHistogramEnumeration("WebAuthentication.CableV1DiscoveryEvent",
                                event);
}

void FidoCableDiscovery::StartInternal() {
  BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &FidoCableDiscovery::OnGetAdapter, weak_factory_.GetWeakPtr()));
}

// static
std::string FidoCableDiscovery::ResultDebugString(
    const CableEidArray& eid,
    const base::Optional<FidoCableDiscovery::Result>& result) {
  static const uint8_t kAppleContinuity[16] = {
      0xd0, 0x61, 0x1e, 0x78, 0xbb, 0xb4, 0x45, 0x91,
      0xa5, 0xf8, 0x48, 0x79, 0x10, 0xae, 0x43, 0x66,
  };
  static const uint8_t kAppleUnknown[16] = {
      0x9f, 0xa4, 0x80, 0xe0, 0x49, 0x67, 0x45, 0x42,
      0x93, 0x90, 0xd3, 0x43, 0xdc, 0x5d, 0x04, 0xae,
  };
  static const uint8_t kAppleMedia[16] = {
      0x89, 0xd3, 0x50, 0x2b, 0x0f, 0x36, 0x43, 0x3a,
      0x8e, 0xf4, 0xc5, 0x02, 0xad, 0x55, 0xf8, 0xdc,
  };
  static const uint8_t kAppleNotificationCenter[16] = {
      0x79, 0x05, 0xf4, 0x31, 0xb5, 0xce, 0x4e, 0x99,
      0xa4, 0x0f, 0x4b, 0x1e, 0x12, 0x2d, 0x00, 0xd0,
  };
  static const uint8_t kCable[16] = {
      0x00, 0x00, 0xfd, 0xe2, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
  };

  std::string ret = base::HexEncode(eid) + "";

  if (!result) {
    // Try to identify some common UUIDs that are random and thus otherwise look
    // like potential EIDs.
    if (memcmp(eid.data(), kAppleContinuity, eid.size()) == 0) {
      ret += " (Apple Continuity service)";
    } else if (memcmp(eid.data(), kAppleUnknown, eid.size()) == 0) {
      ret += " (Apple service)";
    } else if (memcmp(eid.data(), kAppleMedia, eid.size()) == 0) {
      ret += " (Apple Media service)";
    } else if (memcmp(eid.data(), kAppleNotificationCenter, eid.size()) == 0) {
      ret += " (Apple Notification service)";
    } else if (memcmp(eid.data(), kCable, eid.size()) == 0) {
      ret += " (caBLE indicator)";
    }
    return ret;
  }

  switch (result->discovery_data.version) {
    case CableDiscoveryData::Version::V1:
      ret += " (version one match";
      break;
    case CableDiscoveryData::Version::V2:
      ret += " (version two match";
      break;
    case CableDiscoveryData::Version::INVALID:
      NOTREACHED();
  }

  if (!result->ticks_back) {
    ret += " against pairing data)";
  } else {
    ret += " from QR, " + base::NumberToString(*result->ticks_back) +
           " tick(s) ago)";
  }

  return ret;
}

bool FidoCableDiscovery::MaybeStop() {
  if (!FidoDeviceDiscovery::MaybeStop()) {
    NOTREACHED();
  }
  StopAdvertisements(base::DoNothing());
  return true;
}

}  // namespace device
