// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_thread_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/amplifier_client.h"
#include "chromeos/dbus/ap_manager_client.h"
#include "chromeos/dbus/arc_obb_mounter_client.h"
#include "chromeos/dbus/audio_dsp_client.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/introspectable_client.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "chromeos/dbus/modem_messaging_client.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "chromeos/dbus/nfc_device_client.h"
#include "chromeos/dbus/nfc_manager_client.h"
#include "chromeos/dbus/nfc_record_client.h"
#include "chromeos/dbus/nfc_tag_client.h"
#include "chromeos/dbus/peer_daemon_manager_client.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/privet_daemon_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/dbus/shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/sms_client.h"
#include "chromeos/dbus/system_clock_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"

namespace chromeos {

static DBusThreadManager* g_dbus_thread_manager = NULL;
static bool g_using_dbus_thread_manager_for_testing = false;

DBusThreadManager::DBusThreadManager(
    std::unique_ptr<DBusClientBundle> client_bundle)
    : client_bundle_(std::move(client_bundle)) {
  dbus::statistics::Initialize();

  if (client_bundle_->IsUsingAnyRealClient()) {
    // At least one real DBusClient is used.
    // Create the D-Bus thread.
    base::Thread::Options thread_options;
    thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
    dbus_thread_.reset(new base::Thread("D-Bus thread"));
    dbus_thread_->StartWithOptions(thread_options);

    // Create the connection to the system bus.
    dbus::Bus::Options system_bus_options;
    system_bus_options.bus_type = dbus::Bus::SYSTEM;
    system_bus_options.connection_type = dbus::Bus::PRIVATE;
    system_bus_options.dbus_task_runner = dbus_thread_->task_runner();
    system_bus_ = new dbus::Bus(system_bus_options);
  }
}

DBusThreadManager::~DBusThreadManager() {
  // Delete all D-Bus clients before shutting down the system bus.
  client_bundle_.reset();

  // Shut down the bus. During the browser shutdown, it's ok to shut down
  // the bus synchronously.
  if (system_bus_.get())
    system_bus_->ShutdownOnDBusThreadAndBlock();

  // Stop the D-Bus thread.
  if (dbus_thread_)
    dbus_thread_->Stop();

  dbus::statistics::Shutdown();

  if (!g_dbus_thread_manager)
    return;  // Called form Shutdown() or local test instance.

  // There should never be both a global instance and a local instance.
  CHECK(this == g_dbus_thread_manager);
  if (g_using_dbus_thread_manager_for_testing) {
    g_dbus_thread_manager = NULL;
    g_using_dbus_thread_manager_for_testing = false;
    VLOG(1) << "DBusThreadManager destroyed";
  } else {
    LOG(FATAL) << "~DBusThreadManager() called outside of Shutdown()";
  }
}

dbus::Bus* DBusThreadManager::GetSystemBus() {
  return system_bus_.get();
}

AmplifierClient* DBusThreadManager::GetAmplifierClient() {
  return client_bundle_->amplifier_client();
}

ApManagerClient* DBusThreadManager::GetApManagerClient() {
  return client_bundle_->ap_manager_client();
}

ArcObbMounterClient* DBusThreadManager::GetArcObbMounterClient() {
  return client_bundle_->arc_obb_mounter_client();
}

AudioDspClient* DBusThreadManager::GetAudioDspClient() {
  return client_bundle_->audio_dsp_client();
}

CrasAudioClient* DBusThreadManager::GetCrasAudioClient() {
  return client_bundle_->cras_audio_client();
}

CrosDisksClient* DBusThreadManager::GetCrosDisksClient() {
  return client_bundle_->cros_disks_client();
}

CryptohomeClient* DBusThreadManager::GetCryptohomeClient() {
  return client_bundle_->cryptohome_client();
}

DebugDaemonClient* DBusThreadManager::GetDebugDaemonClient() {
  return client_bundle_->debug_daemon_client();
}

EasyUnlockClient* DBusThreadManager::GetEasyUnlockClient() {
  return client_bundle_->easy_unlock_client();
}

LorgnetteManagerClient*
DBusThreadManager::GetLorgnetteManagerClient() {
  return client_bundle_->lorgnette_manager_client();
}

ShillDeviceClient*
DBusThreadManager::GetShillDeviceClient() {
  return client_bundle_->shill_device_client();
}

ShillIPConfigClient*
DBusThreadManager::GetShillIPConfigClient() {
  return client_bundle_->shill_ipconfig_client();
}

ShillManagerClient*
DBusThreadManager::GetShillManagerClient() {
  return client_bundle_->shill_manager_client();
}

ShillServiceClient*
DBusThreadManager::GetShillServiceClient() {
  return client_bundle_->shill_service_client();
}

ShillProfileClient*
DBusThreadManager::GetShillProfileClient() {
  return client_bundle_->shill_profile_client();
}

ShillThirdPartyVpnDriverClient*
DBusThreadManager::GetShillThirdPartyVpnDriverClient() {
  return client_bundle_->shill_third_party_vpn_driver_client();
}

GsmSMSClient* DBusThreadManager::GetGsmSMSClient() {
  return client_bundle_->gsm_sms_client();
}

ImageBurnerClient* DBusThreadManager::GetImageBurnerClient() {
  return client_bundle_->image_burner_client();
}

IntrospectableClient* DBusThreadManager::GetIntrospectableClient() {
  return client_bundle_->introspectable_client();
}

ModemMessagingClient* DBusThreadManager::GetModemMessagingClient() {
  return client_bundle_->modem_messaging_client();
}

NfcAdapterClient* DBusThreadManager::GetNfcAdapterClient() {
  return client_bundle_->nfc_adapter_client();
}

NfcDeviceClient* DBusThreadManager::GetNfcDeviceClient() {
  return client_bundle_->nfc_device_client();
}

NfcManagerClient* DBusThreadManager::GetNfcManagerClient() {
  return client_bundle_->nfc_manager_client();
}

NfcRecordClient* DBusThreadManager::GetNfcRecordClient() {
  return client_bundle_->nfc_record_client();
}

NfcTagClient* DBusThreadManager::GetNfcTagClient() {
  return client_bundle_->nfc_tag_client();
}

PeerDaemonManagerClient* DBusThreadManager::GetPeerDaemonManagerClient() {
  return client_bundle_->peer_daemon_manager_client();
}

PermissionBrokerClient* DBusThreadManager::GetPermissionBrokerClient() {
  return client_bundle_->permission_broker_client();
}

PowerManagerClient* DBusThreadManager::GetPowerManagerClient() {
  return client_bundle_->power_manager_client();
}

PrivetDaemonManagerClient* DBusThreadManager::GetPrivetDaemonManagerClient() {
  return client_bundle_->privet_daemon_manager_client();
}

SessionManagerClient* DBusThreadManager::GetSessionManagerClient() {
  return client_bundle_->session_manager_client();
}

SMSClient* DBusThreadManager::GetSMSClient() {
  return client_bundle_->sms_client();
}

SystemClockClient* DBusThreadManager::GetSystemClockClient() {
  return client_bundle_->system_clock_client();
}

UpdateEngineClient* DBusThreadManager::GetUpdateEngineClient() {
  return client_bundle_->update_engine_client();
}

void DBusThreadManager::InitializeClients() {
  GetAmplifierClient()->Init(GetSystemBus());
  GetApManagerClient()->Init(GetSystemBus());
  GetArcObbMounterClient()->Init(GetSystemBus());
  GetAudioDspClient()->Init(GetSystemBus());
  GetCrasAudioClient()->Init(GetSystemBus());
  GetCrosDisksClient()->Init(GetSystemBus());
  GetCryptohomeClient()->Init(GetSystemBus());
  GetDebugDaemonClient()->Init(GetSystemBus());
  GetEasyUnlockClient()->Init(GetSystemBus());
  GetGsmSMSClient()->Init(GetSystemBus());
  GetImageBurnerClient()->Init(GetSystemBus());
  GetIntrospectableClient()->Init(GetSystemBus());
  GetLorgnetteManagerClient()->Init(GetSystemBus());
  GetModemMessagingClient()->Init(GetSystemBus());
  GetPermissionBrokerClient()->Init(GetSystemBus());
  GetPeerDaemonManagerClient()->Init(GetSystemBus());
  GetPrivetDaemonManagerClient()->Init(GetSystemBus());
  GetPowerManagerClient()->Init(GetSystemBus());
  GetSessionManagerClient()->Init(GetSystemBus());
  GetShillDeviceClient()->Init(GetSystemBus());
  GetShillIPConfigClient()->Init(GetSystemBus());
  GetShillManagerClient()->Init(GetSystemBus());
  GetShillServiceClient()->Init(GetSystemBus());
  GetShillProfileClient()->Init(GetSystemBus());
  GetShillThirdPartyVpnDriverClient()->Init(GetSystemBus());
  GetSMSClient()->Init(GetSystemBus());
  GetSystemClockClient()->Init(GetSystemBus());
  GetUpdateEngineClient()->Init(GetSystemBus());

  // Initialize the NFC clients in the correct order. The order of
  // initialization matters due to dependencies that exist between the
  // client objects.
  GetNfcManagerClient()->Init(GetSystemBus());
  GetNfcAdapterClient()->Init(GetSystemBus());
  GetNfcDeviceClient()->Init(GetSystemBus());
  GetNfcTagClient()->Init(GetSystemBus());
  GetNfcRecordClient()->Init(GetSystemBus());

  // This must be called after the list of clients so they've each had a
  // chance to register with their object g_dbus_thread_managers.
  if (GetSystemBus())
    GetSystemBus()->GetManagedObjects();

  client_bundle_->SetupDefaultEnvironment();
}

bool DBusThreadManager::IsUsingStub(DBusClientBundle::DBusClientType client) {
  return client_bundle_->IsUsingStub(client);
}

// static
void DBusThreadManager::Initialize() {
  // If we initialize DBusThreadManager twice we may also be shutting it down
  // early; do not allow that.
  if (g_using_dbus_thread_manager_for_testing)
    return;

  CHECK(!g_dbus_thread_manager);
  bool use_dbus_stub = !base::SysInfo::IsRunningOnChromeOS() ||
                       base::CommandLine::ForCurrentProcess()->HasSwitch(
                           chromeos::switches::kDbusStub);
  bool force_unstub_clients = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDbusUnstubClients);
  // Determine whether we use stub or real client implementations.
  if (force_unstub_clients) {
    InitializeWithPartialStub(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            chromeos::switches::kDbusUnstubClients));
  } else if (use_dbus_stub) {
    InitializeWithStubs();
  } else {
    InitializeWithRealClients();
  }
}

// static
std::unique_ptr<DBusThreadManagerSetter>
DBusThreadManager::GetSetterForTesting() {
  if (!g_using_dbus_thread_manager_for_testing) {
    g_using_dbus_thread_manager_for_testing = true;
    InitializeWithStubs();
  }

  return base::WrapUnique(new DBusThreadManagerSetter());
}

// static
void DBusThreadManager::CreateGlobalInstance(
    DBusClientBundle::DBusClientTypeMask unstub_client_mask) {
  CHECK(!g_dbus_thread_manager);
  g_dbus_thread_manager = new DBusThreadManager(
      base::WrapUnique(new DBusClientBundle(unstub_client_mask)));
  g_dbus_thread_manager->InitializeClients();
}

// static
void DBusThreadManager::InitializeWithRealClients() {
  CreateGlobalInstance(~static_cast<DBusClientBundle::DBusClientTypeMask>(0));
  VLOG(1) << "DBusThreadManager initialized for Chrome OS";
}

// static
void DBusThreadManager::InitializeWithStubs() {
  CreateGlobalInstance(0 /* unstub_client_mask */);
  VLOG(1) << "DBusThreadManager created for testing";
}

// static
void DBusThreadManager::InitializeWithPartialStub(
    const std::string& unstub_clients) {
  DBusClientBundle::DBusClientTypeMask unstub_client_mask =
      DBusClientBundle::ParseUnstubList(unstub_clients);
  // We should have something parsed correctly here.
  LOG_IF(FATAL, unstub_client_mask == 0)
      << "Switch values for --" << chromeos::switches::kDbusUnstubClients
      << " cannot be parsed: " << unstub_clients;
  VLOG(1) << "DBusThreadManager initialized for mixed runtime environment";
  CreateGlobalInstance(unstub_client_mask);
}

// static
bool DBusThreadManager::IsInitialized() {
  return g_dbus_thread_manager != NULL;
}

// static
void DBusThreadManager::Shutdown() {
  // Ensure that we only shutdown DBusThreadManager once.
  CHECK(g_dbus_thread_manager);
  DBusThreadManager* dbus_thread_manager = g_dbus_thread_manager;
  g_dbus_thread_manager = NULL;
  g_using_dbus_thread_manager_for_testing = false;
  delete dbus_thread_manager;
  VLOG(1) << "DBusThreadManager Shutdown completed";
}

// static
DBusThreadManager* DBusThreadManager::Get() {
  CHECK(g_dbus_thread_manager)
      << "DBusThreadManager::Get() called before Initialize()";
  return g_dbus_thread_manager;
}

DBusThreadManagerSetter::DBusThreadManagerSetter() {
}

DBusThreadManagerSetter::~DBusThreadManagerSetter() {
}

void DBusThreadManagerSetter::SetAmplifierClient(
    std::unique_ptr<AmplifierClient> client) {
  DBusThreadManager::Get()->client_bundle_->amplifier_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetAudioDspClient(
    std::unique_ptr<AudioDspClient> client) {
  DBusThreadManager::Get()->client_bundle_->audio_dsp_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetCrasAudioClient(
    std::unique_ptr<CrasAudioClient> client) {
  DBusThreadManager::Get()->client_bundle_->cras_audio_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetCrosDisksClient(
    std::unique_ptr<CrosDisksClient> client) {
  DBusThreadManager::Get()->client_bundle_->cros_disks_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetCryptohomeClient(
    std::unique_ptr<CryptohomeClient> client) {
  DBusThreadManager::Get()->client_bundle_->cryptohome_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetDebugDaemonClient(
    std::unique_ptr<DebugDaemonClient> client) {
  DBusThreadManager::Get()->client_bundle_->debug_daemon_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetEasyUnlockClient(
    std::unique_ptr<EasyUnlockClient> client) {
  DBusThreadManager::Get()->client_bundle_->easy_unlock_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetLorgnetteManagerClient(
    std::unique_ptr<LorgnetteManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->lorgnette_manager_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetShillDeviceClient(
    std::unique_ptr<ShillDeviceClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_device_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetShillIPConfigClient(
    std::unique_ptr<ShillIPConfigClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_ipconfig_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetShillManagerClient(
    std::unique_ptr<ShillManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_manager_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetShillServiceClient(
    std::unique_ptr<ShillServiceClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_service_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetShillProfileClient(
    std::unique_ptr<ShillProfileClient> client) {
  DBusThreadManager::Get()->client_bundle_->shill_profile_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetShillThirdPartyVpnDriverClient(
    std::unique_ptr<ShillThirdPartyVpnDriverClient> client) {
  DBusThreadManager::Get()
      ->client_bundle_->shill_third_party_vpn_driver_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetGsmSMSClient(
    std::unique_ptr<GsmSMSClient> client) {
  DBusThreadManager::Get()->client_bundle_->gsm_sms_client_ = std::move(client);
}

void DBusThreadManagerSetter::SetImageBurnerClient(
    std::unique_ptr<ImageBurnerClient> client) {
  DBusThreadManager::Get()->client_bundle_->image_burner_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetIntrospectableClient(
    std::unique_ptr<IntrospectableClient> client) {
  DBusThreadManager::Get()->client_bundle_->introspectable_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetModemMessagingClient(
    std::unique_ptr<ModemMessagingClient> client) {
  DBusThreadManager::Get()->client_bundle_->modem_messaging_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetNfcAdapterClient(
    std::unique_ptr<NfcAdapterClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_adapter_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetNfcDeviceClient(
    std::unique_ptr<NfcDeviceClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_device_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetNfcManagerClient(
    std::unique_ptr<NfcManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_manager_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetNfcRecordClient(
    std::unique_ptr<NfcRecordClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_record_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetNfcTagClient(
    std::unique_ptr<NfcTagClient> client) {
  DBusThreadManager::Get()->client_bundle_->nfc_tag_client_ = std::move(client);
}

void DBusThreadManagerSetter::SetPeerDaemonManagerClient(
    std::unique_ptr<PeerDaemonManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->peer_daemon_manager_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetPermissionBrokerClient(
    std::unique_ptr<PermissionBrokerClient> client) {
  DBusThreadManager::Get()->client_bundle_->permission_broker_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetPrivetDaemonManagerClient(
    std::unique_ptr<PrivetDaemonManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->privet_daemon_manager_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetPowerManagerClient(
    std::unique_ptr<PowerManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->power_manager_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetSessionManagerClient(
    std::unique_ptr<SessionManagerClient> client) {
  DBusThreadManager::Get()->client_bundle_->session_manager_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetSMSClient(std::unique_ptr<SMSClient> client) {
  DBusThreadManager::Get()->client_bundle_->sms_client_ = std::move(client);
}

void DBusThreadManagerSetter::SetSystemClockClient(
    std::unique_ptr<SystemClockClient> client) {
  DBusThreadManager::Get()->client_bundle_->system_clock_client_ =
      std::move(client);
}

void DBusThreadManagerSetter::SetUpdateEngineClient(
    std::unique_ptr<UpdateEngineClient> client) {
  DBusThreadManager::Get()->client_bundle_->update_engine_client_ =
      std::move(client);
}

}  // namespace chromeos
