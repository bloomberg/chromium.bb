// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_SERVICE_CONSTANTS_H_
#define DBUS_SERVICE_CONSTANTS_H_

namespace dbus {
const char kDBusPropertiesInterface[] = "org.freedesktop.DBus.Properties";
// Methods
const char kDBusPropertiesGetAll[] = "GetAll";
}  // namespace dbus


namespace cryptohome {
const char kCryptohomeInterface[] = "org.chromium.CryptohomeInterface";
const char kCryptohomeServicePath[] = "/org/chromium/Cryptohome";
const char kCryptohomeServiceName[] = "org.chromium.Cryptohome";
// Methods
const char kCryptohomeCheckKey[] = "CheckKey";
const char kCryptohomeMigrateKey[] = "MigrateKey";
const char kCryptohomeRemove[] = "Remove";
const char kCryptohomeGetSystemSalt[] = "GetSystemSalt";
const char kCryptohomeIsMounted[] = "IsMounted";
const char kCryptohomeMount[] = "Mount";
const char kCryptohomeMountGuest[] = "MountGuest";
const char kCryptohomeUnmount[] = "Unmount";
const char kCryptohomeTpmIsReady[] = "TpmIsReady";
const char kCryptohomeTpmIsEnabled[] = "TpmIsEnabled";
const char kCryptohomeTpmIsOwned[] = "TpmIsOwned";
const char kCryptohomeTpmIsBeingOwned[] = "TpmIsBeingOwned";
const char kCryptohomeTpmGetPassword[] = "TpmGetPassword";
const char kCryptohomeTpmCanAttemptOwnership[] = "TpmCanAttemptOwnership";
const char kCryptohomeTpmClearStoredPassword[] = "TpmClearStoredPassword";
const char kCryptohomePkcs11GetTpmTokenInfo[] = "Pkcs11GetTpmTokenInfo";
const char kCryptohomePkcs11GetTpmTokenInfoForUser[] =
    "Pkcs11GetTpmTokenInfoForUser";
const char kCryptohomePkcs11IsTpmTokenReady[] = "Pkcs11IsTpmTokenReady";
const char kCryptohomePkcs11IsTpmTokenReadyForUser[] =
    "Pkcs11IsTpmTokenReadyForUser";
const char kCryptohomeAsyncCheckKey[] = "AsyncCheckKey";
const char kCryptohomeAsyncMigrateKey[] = "AsyncMigrateKey";
const char kCryptohomeAsyncMount[] = "AsyncMount";
const char kCryptohomeAsyncMountGuest[] = "AsyncMountGuest";
const char kCryptohomeAsyncRemove[] = "AsyncRemove";
const char kCryptohomeGetStatusString[] = "GetStatusString";
const char kCryptohomeRemoveTrackedSubdirectories[] =
    "RemoveTrackedSubdirectories";
const char kCryptohomeAsyncRemoveTrackedSubdirectories[] =
    "AsyncRemoveTrackedSubdirectories";
const char kCryptohomeDoAutomaticFreeDiskSpaceControl[] =
    "DoAutomaticFreeDiskSpaceControl";
const char kCryptohomeAsyncDoAutomaticFreeDiskSpaceControl[] =
    "AsyncDoAutomaticFreeDiskSpaceControl";
const char kCryptohomeAsyncDoesUsersExist[] = "AsyncDoesUsersExist";
const char kCryptohomeInstallAttributesGet[] = "InstallAttributesGet";
const char kCryptohomeInstallAttributesSet[] = "InstallAttributesSet";
const char kCryptohomeInstallAttributesCount[] = "InstallAttributesCount";
const char kCryptohomeInstallAttributesFinalize[] =
    "InstallAttributesFinalize";
const char kCryptohomeInstallAttributesIsReady[] = "InstallAttributesIsReady";
const char kCryptohomeInstallAttributesIsSecure[] =
    "InstallAttributesIsSecure";
const char kCryptohomeInstallAttributesIsInvalid[] =
    "InstallAttributesIsInvalid";
const char kCryptohomeInstallAttributesIsFirstInstall[] =
    "InstallAttributesIsFirstInstall";
// Signals
const char kSignalAsyncCallStatus[] = "AsyncCallStatus";
const char kSignalTpmInitStatus[] = "TpmInitStatus";
const char kSignalCleanupUsersRemoved[] = "CleanupUsersRemoved";
// Error code
enum MountError {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_FATAL = 1 << 0,
  MOUNT_ERROR_KEY_FAILURE = 1 << 1,
  MOUNT_ERROR_MOUNT_POINT_BUSY = 1 << 2,
  MOUNT_ERROR_TPM_COMM_ERROR = 1 << 3,
  MOUNT_ERROR_TPM_DEFEND_LOCK = 1 << 4,
  MOUNT_ERROR_USER_DOES_NOT_EXIST = 1 << 5,
  MOUNT_ERROR_RECREATED = 1 << 31,
};
}  // namespace cryptohome

namespace imageburn {
const char kImageBurnServiceName[] = "org.chromium.ImageBurner";
const char kImageBurnServicePath[] = "/org/chromium/ImageBurner";
const char kImageBurnServiceInterface[] = "org.chromium.ImageBurnerInterface";
// Methods
const char kBurnImage[] = "BurnImage";
// Signals
const char kSignalBurnFinishedName[] = "burn_finished";
const char kSignalBurnUpdateName[] = "burn_progress_update";
}  // namespace imageburn

namespace login_manager {
const char kSessionManagerInterface[] = "org.chromium.SessionManagerInterface";
const char kSessionManagerServicePath[] = "/org/chromium/SessionManager";
const char kSessionManagerServiceName[] = "org.chromium.SessionManager";
// Methods
const char kSessionManagerEmitLoginPromptReady[] = "EmitLoginPromptReady";
const char kSessionManagerEmitLoginPromptVisible[] = "EmitLoginPromptVisible";
const char kSessionManagerStartSession[] = "StartSession";
const char kSessionManagerStopSession[] = "StopSession";
const char kSessionManagerRestartJob[] = "RestartJob";
const char kSessionManagerRestartEntd[] = "RestartEntd";
const char kSessionManagerSetOwnerKey[] = "SetOwnerKey";
const char kSessionManagerUnwhitelist[] = "Unwhitelist";
const char kSessionManagerCheckWhitelist[] = "CheckWhitelist";
const char kSessionManagerEnumerateWhitelisted[] = "EnumerateWhitelisted";
const char kSessionManagerWhitelist[] = "Whitelist";
const char kSessionManagerStoreProperty[] = "StoreProperty";
const char kSessionManagerRetrieveProperty[] = "RetrieveProperty";
const char kSessionManagerStorePolicy[] = "StorePolicy";
const char kSessionManagerRetrievePolicy[] = "RetrievePolicy";
const char kSessionManagerStoreUserPolicy[] = "StoreUserPolicy";
const char kSessionManagerRetrieveUserPolicy[] = "RetrieveUserPolicy";
const char kSessionManagerRetrieveSessionState[] = "RetrieveSessionState";
const char kSessionManagerStartSessionService[] = "StartSessionService";
const char kSessionManagerStopSessionService[] = "StopSessionService";
// Signals
const char kSessionManagerSessionStateChanged[] = "SessionStateChanged";
}  // namespace login_manager

namespace speech_synthesis {
const char kSpeechSynthesizerInterface[] =
    "org.chromium.SpeechSynthesizerInterface";
const char kSpeechSynthesizerServicePath[] = "/org/chromium/SpeechSynthesizer";
const char kSpeechSynthesizerServiceName[] = "org.chromium.SpeechSynthesizer";
// Methods
const char kSpeak[] = "Speak";
const char kStop[] = "Stop";
const char kIsSpeaking[] = "IsSpeaking";
const char kShutdown[] = "Shutdown";
}  // namespace speech_synthesis

namespace chromium {
const char kChromiumInterface[] = "org.chromium.Chromium";
// ScreenLock signals.
const char kLockScreenSignal[] = "LockScreen";
const char kUnlockScreenSignal[] = "UnlockScreen";
const char kUnlockScreenFailedSignal[] = "UnlockScreenFailed";
// Text-to-speech service signals
const char kTTSReadySignal[] = "TTSReady";
const char kTTSFailedSignal[] = "TTSFailed";
// Ownership API signals
const char kOwnerKeySetSignal[] = "SetOwnerKeyComplete";
const char kPropertyChangeCompleteSignal[] = "PropertyChangeComplete";
const char kWhitelistChangeCompleteSignal[] = "WhitelistChangeComplete";
}  // namespace chromium

namespace power_manager {
const char kPowerManagerInterface[] = "org.chromium.PowerManager";
const char kPowerManagerServicePath[] = "/org/chromium/PowerManager";
const char kPowerManagerServiceName[] = "org.chromium.PowerManager";
// Methods
const char kDecreaseScreenBrightness[] = "DecreaseScreenBrightness";
const char kIncreaseScreenBrightness[] = "IncreaseScreenBrightness";
const char kGetScreenBrightnessPercent[] = "GetScreenBrightnessPercent";
const char kSetScreenBrightnessPercent[] = "SetScreenBrightnessPercent";
const char kDecreaseKeyboardBrightness[] = "DecreaseKeyboardBrightness";
const char kIncreaseKeyboardBrightness[] = "IncreaseKeyboardBrightness";
const char kGetIdleTime[] = "GetIdleTime";
const char kRequestIdleNotification[] = "RequestIdleNotification";
const char kRequestLockScreenMethod[] = "RequestLockScreen";
const char kRequestRestartMethod[] = "RequestRestart";
const char kRequestShutdownMethod[] = "RequestShutdown";
const char kRequestUnlockScreenMethod[] = "RequestUnlockScreen";
const char kScreenIsLockedMethod[] = "ScreenIsLocked";
const char kScreenIsUnlockedMethod[] = "ScreenIsUnlocked";
const char kGetPowerSupplyPropertiesMethod[] = "GetPowerSupplyProperties";
const char kStateOverrideRequest[] = "StateOverrideRequest";
const char kGetPowerAccumulationMethod[] = "GetPowerAccumulation";
// DEPRECATED
const char kPowerManagerDecreaseScreenBrightness[] =
    "DecreaseScreenBrightness";
const char kPowerManagerIncreaseScreenBrightness[] =
    "IncreaseScreenBrightness";
const char kPowerManagerDecreaseKeyboardBrightness[] =
    "DecreaseKeyboardBrightness";
const char kPowerManagerIncreaseKeyboardBrightness[] =
    "IncreaseKeyboardBrightness";
// Signals
const char kRequestSuspendSignal[] = "RequestSuspend";
const char kCleanShutdown[] = "CleanShutdown";
const char kRegisterSuspendDelay[] = "RegisterSuspendDelay";
const char kUnregisterSuspendDelay[] = "UnregisterSuspendDelay";
const char kSuspendDelay[] = "SuspendDelay";
const char kSuspendReady[] = "SuspendReady";
const char kBrightnessChangedSignal[] = "BrightnessChanged";
const char kIdleNotifySignal[] = "IdleNotify";
const char kActiveNotifySignal[] = "ActiveNotify";
const char kKeyboardBrightnessChangedSignal[] = "KeyboardBrightnessChanged";
const char kPowerStateChangedSignal[] = "PowerStateChanged";
const char kPowerSupplyPollSignal[] = "PowerSupplyPoll";
const char kButtonEventSignal[] = "ButtonEvent";
const char kStateOverrideCancel[] = "StateOverrideCancel";
const char kSoftwareScreenDimmingRequestedSignal[] =
    "SoftwareScreenDimmingRequested";
// kUseNewMonitorConfigSignal is temporary until Chrome monitor configuration
// lands.
const char kUseNewMonitorConfigSignal[] = "UseNewMonitorConfigSignal";
const char kSetScreenPowerSignal[] = "SetScreenPowerSignal";
// Values
const char kPowerButtonName[] = "power";
const char kLockButtonName[] = "lock";
const int  kBrightnessTransitionGradual = 1;
const int  kBrightnessTransitionInstant = 2;
const int  kSoftwareScreenDimmingNone = 1;
const int  kSoftwareScreenDimmingIdle = 2;
// DEPRECATED
const char kRequestLockScreenSignal[] = "RequestLockScreen";
const char kRequestRestartSignal[] = "RequestRestart";
const char kRequestShutdownSignal[] = "RequestShutdown";
const char kRequestUnlockScreenSignal[] = "RequestUnlockScreen";
const char kScreenIsLockedSignal[] = "ScreenIsLocked";
const char kScreenIsUnlockedSignal[] = "ScreenIsUnlocked";
}  // namespace power_manager

namespace chromeos {
const char kLibCrosServiceName[] = "org.chromium.LibCrosService";
const char kLibCrosServicePath[] = "/org/chromium/LibCrosService";
const char kLibCrosServiceInterface[] = "org.chromium.LibCrosServiceInterface";
// Methods
const char kResolveNetworkProxy[] = "ResolveNetworkProxy";
}  // namespace chromeos

namespace flimflam {
// Flimflam D-Bus service identifiers.
const char kFlimflamManagerInterface[] = "org.chromium.flimflam.Manager";
const char kFlimflamServiceName[] = "org.chromium.flimflam";
const char kFlimflamServicePath[] = "/";  // crosbug.com/20135
const char kFlimflamServiceInterface[] = "org.chromium.flimflam.Service";
const char kFlimflamIPConfigInterface[] = "org.chromium.flimflam.IPConfig";
const char kFlimflamDeviceInterface[] = "org.chromium.flimflam.Device";
const char kFlimflamProfileInterface[] = "org.chromium.flimflam.Profile";
const char kFlimflamNetworkInterface[] = "org.chromium.flimflam.Network";

// Flimflam function names.
const char kGetPropertiesFunction[] = "GetProperties";
const char kSetPropertyFunction[] = "SetProperty";
const char kClearPropertyFunction[] = "ClearProperty";
const char kConnectFunction[] = "Connect";
const char kDisconnectFunction[] = "Disconnect";
const char kRequestScanFunction[] = "RequestScan";
const char kGetServiceFunction[] = "GetService";
const char kGetWifiServiceFunction[] = "GetWifiService";
const char kGetVPNServiceFunction[] = "GetVPNService";
const char kRemoveServiceFunction[] = "Remove";
const char kEnableTechnologyFunction[] = "EnableTechnology";
const char kDisableTechnologyFunction[] = "DisableTechnology";
const char kAddIPConfigFunction[] = "AddIPConfig";
const char kRemoveConfigFunction[] = "Remove";
const char kGetEntryFunction[] = "GetEntry";
const char kDeleteEntryFunction[] = "DeleteEntry";
const char kActivateCellularModemFunction[] = "ActivateCellularModem";
const char kRequirePinFunction[] = "RequirePin";
const char kEnterPinFunction[] = "EnterPin";
const char kUnblockPinFunction[] = "UnblockPin";
const char kChangePinFunction[] = "ChangePin";
const char kProposeScanFunction[] = "ProposeScan";
const char kRegisterFunction[] = "Register";
const char kConfigureServiceFunction[] = "ConfigureService";
const char kConfigureWifiServiceFunction[] = "ConfigureWifiService";

// Flimflam Service property names.
const char kSecurityProperty[] = "Security";
const char kPriorityProperty[] = "Priority";
const char kPassphraseProperty[] = "Passphrase";
const char kIdentityProperty[] = "Identity";
const char kAuthorityPathProperty[] = "AuthorityPath";
const char kPassphraseRequiredProperty[] = "PassphraseRequired";
const char kSaveCredentialsProperty[] = "SaveCredentials";
const char kSignalStrengthProperty[] = "Strength";
const char kNameProperty[] = "Name";
const char kGuidProperty[] = "GUID";
const char kStateProperty[] = "State";
const char kTypeProperty[] = "Type";
const char kDeviceProperty[] = "Device";
const char kProfileProperty[] = "Profile";
const char kConnectivityStateProperty[] = "ConnectivityState";
const char kFavoriteProperty[] = "Favorite";
const char kConnectableProperty[] = "Connectable";
const char kAutoConnectProperty[] = "AutoConnect";
const char kIsActiveProperty[] = "IsActive";
const char kModeProperty[] = "Mode";
const char kErrorProperty[] = "Error";
const char kProviderProperty[] = "Provider";
const char kHostProperty[] = "Host";
const char kDomainProperty[] = "Domain";
const char kProxyConfigProperty[] = "ProxyConfig";
const char kCheckPortalProperty[] = "CheckPortal";
const char kSSIDProperty[] = "SSID";
const char kConnectedProperty[] = "Connected";
const char kUIDataProperty[] = "UIData";

// Flimflam provider property names.
const char kProviderHostProperty[] = "Provider.Host";
const char kProviderNameProperty[] = "Provider.Name";
const char kProviderTypeProperty[] = "Provider.Type";

// Flimflam Wifi Service property names.
const char kWifiBSsid[] = "WiFi.BSSID";
const char kWifiHexSsid[] = "WiFi.HexSSID";
const char kWifiFrequency[] = "WiFi.Frequency";
const char kWifiHiddenSsid[] = "WiFi.HiddenSSID";
const char kWifiPhyMode[] = "WiFi.PhyMode";
const char kWifiAuthMode[] = "WiFi.AuthMode";
const char kWifiChannelProperty[] = "WiFi.Channel";

// Flimflam EAP property names.
const char kEapIdentityProperty[] = "EAP.Identity";
const char kEapMethodProperty[] = "EAP.EAP";
const char kEapPhase2AuthProperty[] = "EAP.InnerEAP";
const char kEapAnonymousIdentityProperty[] = "EAP.AnonymousIdentity";
const char kEapClientCertProperty[] = "EAP.ClientCert";
const char kEapCertIdProperty[] = "EAP.CertID";
const char kEapClientCertNssProperty[] = "EAP.ClientCertNSS";
const char kEapPrivateKeyProperty[] = "EAP.PrivateKey";
const char kEapPrivateKeyPasswordProperty[] = "EAP.PrivateKeyPassword";
const char kEapKeyIdProperty[] = "EAP.KeyID";
const char kEapCaCertProperty[] = "EAP.CACert";
const char kEapCaCertIdProperty[] = "EAP.CACertID";
const char kEapCaCertNssProperty[] = "EAP.CACertNSS";
const char kEapUseSystemCasProperty[] = "EAP.UseSystemCAs";
const char kEapPinProperty[] = "EAP.PIN";
const char kEapPasswordProperty[] = "EAP.Password";
const char kEapKeyMgmtProperty[] = "EAP.KeyMgmt";
// Deprecated (duplicates)
const char kEAPEAPProperty[] = "EAP.EAP";
const char kEAPClientCertProperty[] = "EAP.ClientCert";
const char kEAPCertIDProperty[] = "EAP.CertID";
const char kEAPKeyIDProperty[] = "EAP.KeyID";
const char kEapCaCertIDProperty[] = "EAP.CACertID";
const char kEapUseSystemCAsProperty[] = "EAP.UseSystemCAs";
const char kEAPPINProperty[] = "EAP.PIN";


// Flimflam Cellular Service property names.
const char kTechnologyFamilyProperty[] = "Cellular.Family";
const char kActivationStateProperty[] = "Cellular.ActivationState";
const char kNetworkTechnologyProperty[] = "Cellular.NetworkTechnology";
const char kRoamingStateProperty[] = "Cellular.RoamingState";
const char kOperatorNameProperty[] = "Cellular.OperatorName";
const char kOperatorCodeProperty[] = "Cellular.OperatorCode";
const char kServingOperatorProperty[] = "Cellular.ServingOperator";
// DEPRECATED
const char kPaymentURLProperty[] = "Cellular.OlpUrl";
const char kPaymentPortalProperty[] = "Cellular.Olp";
const char kUsageURLProperty[] = "Cellular.UsageUrl";
const char kCellularApnProperty[] = "Cellular.APN";
const char kCellularLastGoodApnProperty[] = "Cellular.LastGoodAPN";
const char kCellularApnListProperty[] = "Cellular.APNList";

// Flimflam Manager property names.
const char kProfilesProperty[] = "Profiles";
const char kServicesProperty[] = "Services";
const char kServiceWatchListProperty[] = "ServiceWatchList";
const char kAvailableTechnologiesProperty[] = "AvailableTechnologies";
const char kEnabledTechnologiesProperty[] = "EnabledTechnologies";
const char kConnectedTechnologiesProperty[] = "ConnectedTechnologies";
const char kDefaultTechnologyProperty[] = "DefaultTechnology";
const char kOfflineModeProperty[] = "OfflineMode";
const char kActiveProfileProperty[] = "ActiveProfile";
const char kDevicesProperty[] = "Devices";
const char kCheckPortalListProperty[] = "CheckPortalList";
const char kArpGatewayProperty[] = "ArpGateway";
const char kCountryProperty[] = "Country";
const char kPortalURLProperty[] = "PortalURL";

// Flimflam Profile property names.
const char kEntriesProperty[] = "Entries";

// Flimflam Device property names.
const char kScanningProperty[] = "Scanning";
const char kPoweredProperty[] = "Powered";
const char kNetworksProperty[] = "Networks";
const char kScanIntervalProperty[] = "ScanInterval";
const char kBgscanMethodProperty[] = "BgscanMethod";
const char kBgscanShortIntervalProperty[] = "BgscanShortInterval";
const char kDBusConnectionProperty[] = "DBus.Connection";
const char kDBusObjectProperty[] = "DBus.Object";
const char kBgscanSignalThresholdProperty[] = "BgscanSignalThreshold";
// The name of the network interface, ie. wlan0, eth0, etc.
const char kInterfaceProperty[] = "Interface";

// Flimflam Cellular Device property names.
const char kCarrierProperty[] = "Cellular.Carrier";
const char kCellularAllowRoamingProperty[] = "Cellular.AllowRoaming";
const char kHomeProviderProperty[] = "Cellular.HomeProvider";
const char kMeidProperty[] = "Cellular.MEID";
const char kImeiProperty[] = "Cellular.IMEI";
const char kImsiProperty[] = "Cellular.IMSI";
const char kEsnProperty[] = "Cellular.ESN";
const char kMdnProperty[] = "Cellular.MDN";
const char kMinProperty[] = "Cellular.MIN";
const char kModelIDProperty[] = "Cellular.ModelID";
const char kManufacturerProperty[] = "Cellular.Manufacturer";
const char kFirmwareRevisionProperty[] = "Cellular.FirmwareRevision";
const char kHardwareRevisionProperty[] = "Cellular.HardwareRevision";
const char kPRLVersionProperty[] = "Cellular.PRLVersion";
const char kSelectedNetworkProperty[] = "Cellular.SelectedNetwork";
const char kSupportNetworkScanProperty[] = "Cellular.SupportNetworkScan";
const char kFoundNetworksProperty[] = "Cellular.FoundNetworks";
const char kIPConfigsProperty[] = "IPConfigs";

// Flimflam state options.
const char kStateIdle[] = "idle";
const char kStateCarrier[] = "carrier";
const char kStateAssociation[] = "association";
const char kStateConfiguration[] = "configuration";
const char kStateReady[] = "ready";
const char kStatePortal[] = "portal";
const char kStateOffline[] = "offline";
const char kStateOnline[] = "online";
const char kStateDisconnect[] = "disconnect";
const char kStateFailure[] = "failure";
const char kStateActivationFailure[] = "activation-failure";

// Flimflam property names for SIMLock status.
const char kSIMLockStatusProperty[] = "Cellular.SIMLockStatus";
const char kSIMLockTypeProperty[] = "LockType";
const char kSIMLockRetriesLeftProperty[] = "RetriesLeft";
const char kSIMLockEnabledProperty[] = "LockEnabled";

// Flimflam property names for Cellular.FoundNetworks.
const char kLongNameProperty[] = "long_name";
const char kStatusProperty[] = "status";
const char kShortNameProperty[] = "short_name";
const char kTechnologyProperty[] = "technology";
const char kNetworkIdProperty[] = "network_id";

// Flimflam SIMLock status types.
const char kSIMLockPin[] = "sim-pin";
const char kSIMLockPuk[] = "sim-puk";

// APN info property names.
const char kApnProperty[] = "apn";
const char kApnNetworkIdProperty[] = "network_id";
const char kApnUsernameProperty[] = "username";
const char kApnPasswordProperty[] = "password";
const char kApnNameProperty[] = "name";
const char kApnLocalizedNameProperty[] = "localized_name";
const char kApnLanguageProperty[] = "language";

// Payment Portal property names.
const char kPaymentPortalURL[] = "url";
const char kPaymentPortalMethod[] = "method";
const char kPaymentPortalPostData[] = "postdata";

// Operator info property names.
const char kOperatorNameKey[] = "name";
const char kOperatorCodeKey[] = "code";
const char kOperatorCountryKey[] = "country";

// Flimflam network technology options.
const char kNetworkTechnology1Xrtt[] = "1xRTT";
const char kNetworkTechnologyEvdo[] = "EVDO";
const char kNetworkTechnologyGsm[] = "GSM";
const char kNetworkTechnologyGprs[] = "GPRS";
const char kNetworkTechnologyEdge[] = "EDGE";
const char kNetworkTechnologyUmts[] = "UMTS";
const char kNetworkTechnologyHspa[] = "HSPA";
const char kNetworkTechnologyHspaPlus[] = "HSPA+";
const char kNetworkTechnologyLte[] = "LTE";
const char kNetworkTechnologyLteAdvanced[] = "LTE Advanced";

// Flimflam roaming state options
const char kRoamingStateHome[] = "home";
const char kRoamingStateRoaming[] = "roaming";
const char kRoamingStateUnknown[] = "unknown";

// Flimflam activation state options
const char kActivationStateActivated[] = "activated";
const char kActivationStateActivating[] = "activating";
const char kActivationStateNotActivated[] = "not-activated";
const char kActivationStatePartiallyActivated[] = "partially-activated";
const char kActivationStateUnknown[] = "unknown";

// Flimflam EAP method options.
const char kEapMethodPEAP[] = "PEAP";
const char kEapMethodTLS[] = "TLS";
const char kEapMethodTTLS[] = "TTLS";
const char kEapMethodLEAP[] = "LEAP";

// Flimflam EAP phase 2 auth options.
const char kEapPhase2AuthPEAPMD5[] = "auth=MD5";
const char kEapPhase2AuthPEAPMSCHAPV2[] = "auth=MSCHAPV2";
const char kEapPhase2AuthTTLSMD5[] = "autheap=MD5";  // crosbug/26822
const char kEapPhase2AuthTTLSEAPMD5[] = "autheap=MD5";
const char kEapPhase2AuthTTLSEAPMSCHAPV2[] = "autheap=MSCHAPV2";
const char kEapPhase2AuthTTLSMSCHAPV2[] = "auth=MSCHAPV2";
const char kEapPhase2AuthTTLSMSCHAP[] = "auth=MSCHAP";
const char kEapPhase2AuthTTLSPAP[] = "auth=PAP";
const char kEapPhase2AuthTTLSCHAP[] = "auth=CHAP";

// Flimflam VPN provider types.
const char kProviderL2tpIpsec[] = "l2tpipsec";
const char kProviderOpenVpn[] = "openvpn";

// Flimflam VPN service properties
const char kVPNDomainProperty[] = "VPN.Domain";

// Flimflam monitored properties
const char kMonitorPropertyChanged[] = "PropertyChanged";

// Flimflam type options.
const char kTypeEthernet[] = "ethernet";
const char kTypeWifi[] = "wifi";
const char kTypeWimax[] = "wimax";
const char kTypeBluetooth[] = "bluetooth";
const char kTypeCellular[] = "cellular";
const char kTypeVPN[] = "vpn";

// Flimflam mode options.
const char kModeManaged[] = "managed";
const char kModeAdhoc[] = "adhoc";

// Flimflam security options.
const char kSecurityWpa[] = "wpa";
const char kSecurityWep[] = "wep";
const char kSecurityRsn[] = "rsn";
const char kSecurity8021x[] = "802_1x";
const char kSecurityPsk[] = "psk";
const char kSecurityNone[] = "none";

// Flimflam L2TPIPsec property names.
const char kL2tpIpsecAuthenticationType[] = "L2TPIPsec.AuthenticationType";
const char kL2tpIpsecCaCertNssProperty[] = "L2TPIPsec.CACertNSS";
const char kL2tpIpsecClientCertIdProperty[] = "L2TPIPsec.ClientCertID";
const char kL2tpIpsecClientCertSlotProperty[] = "L2TPIPsec.ClientCertSlot";
const char kL2tpIpsecIkeVersion[] = "L2TPIPsec.IKEVersion";
const char kL2tpIpsecPinProperty[] = "L2TPIPsec.PIN";
const char kL2tpIpsecPskProperty[] = "L2TPIPsec.PSK";
const char kL2tpIpsecPskRequiredProperty[] = "L2TPIPsec.PSKRequired";
const char kL2tpIpsecUserProperty[] = "L2TPIPsec.User";
const char kL2tpIpsecPasswordProperty[] = "L2TPIPsec.Password";
const char kL2tpIpsecGroupNameProperty[] = "L2TPIPsec.GroupName";
// deprecated:
const char kL2tpIpsecClientCertSlotProp[] = "L2TPIPsec.ClientCertSlot";

// Flimflam OpenVPN property names.
const char kOpenVPNAuthNoCacheProperty[] = "OpenVPN.AuthNoCache";
const char kOpenVPNAuthProperty[] = "OpenVPN.Auth";
const char kOpenVPNAuthRetryProperty[] = "OpenVPN.AuthRetry";
const char kOpenVPNAuthUserPassProperty[] = "OpenVPN.AuthUserPass";
const char kOpenVPNCaCertProperty[] = "OpenVPN.CACert";
const char kOpenVPNCaCertNSSProperty[] = "OpenVPN.CACertNSS";
const char kOpenVPNClientCertIdProperty[] = "OpenVPN.Pkcs11.ID";
const char kOpenVPNClientCertSlotProperty[] = "OpenVPN.Pkcs11.Slot";
const char kOpenVPNCipherProperty[] = "OpenVPN.Cipher";
const char kOpenVPNCompLZOProperty[] = "OpenVPN.CompLZO";
const char kOpenVPNCompNoAdaptProperty[] = "OpenVPN.CompNoAdapt";
const char kOpenVPNKeyDirectionProperty[] = "OpenVPN.KeyDirection";
const char kOpenVPNMgmtEnableProperty[] = "OpenVPN.Mgmt.Enable";
const char kOpenVPNNsCertTypeProperty[] = "OpenVPN.NsCertType";
const char kOpenVPNOTPProperty[] = "OpenVPN.OTP";
const char kOpenVPNPasswordProperty[] = "OpenVPN.Password";
const char kOpenVPNPinProperty[] = "OpenVPN.Pkcs11.PIN";
const char kOpenVPNPortProperty[] = "OpenVPN.Port";
const char kOpenVPNProtoProperty[] = "OpenVPN.Proto";
const char kOpenVPNProviderProperty[] = "OpenVPN.Pkcs11.Provider";
const char kOpenVPNPushPeerInfoProperty[] = "OpenVPN.PushPeerInfo";
const char kOpenVPNRemoteCertEKUProperty[] = "OpenVPN.RemoteCertEKU";
const char kOpenVPNRemoteCertKUProperty[] = "OpenVPN.RemoteCertKU";
const char kOpenVPNRemoteCertTLSProperty[] = "OpenVPN.RemoteCertTLS";
const char kOpenVPNRenegSecProperty[] = "OpenVPN.RenegSec";
const char kOpenVPNServerPollTimeoutProperty[] = "OpenVPN.ServerPollTimeout";
const char kOpenVPNShaperProperty[] = "OpenVPN.Shaper";
const char kOpenVPNStaticChallengeProperty[] = "OpenVPN.StaticChallenge";
const char kOpenVPNTLSAuthContentsProperty[] = "OpenVPN.TLSAuthContents";
const char kOpenVPNTLSRemoteProperty[] = "OpenVPN.TLSRemote";
const char kOpenVPNUserProperty[] = "OpenVPN.User";

// FlimFlam technology family options
const char kTechnologyFamilyCdma[] = "CDMA";
const char kTechnologyFamilyGsm[] = "GSM";

// IPConfig property names.
const char kMethodProperty[] = "Method";
const char kAddressProperty[] = "Address";
const char kMtuProperty[] = "Mtu";
const char kPrefixlenProperty[] = "Prefixlen";
const char kBroadcastProperty[] = "Broadcast";
const char kPeerAddressProperty[] = "PeerAddress";
const char kGatewayProperty[] = "Gateway";
const char kDomainNameProperty[] = "DomainName";
const char kNameServersProperty[] = "NameServers";

// IPConfig type options.
const char kTypeIPv4[] = "ipv4";
const char kTypeIPv6[] = "ipv6";
const char kTypeDHCP[] = "dhcp";
const char kTypeBOOTP[] = "bootp";
const char kTypeZeroConf[] = "zeroconf";
const char kTypeDHCP6[] = "dhcp6";
const char kTypePPP[] = "ppp";

// Flimflam error options.
const char kErrorAaaFailed[] = "aaa-failed";
const char kErrorActivationFailed[] = "activation-failed";
const char kErrorBadPassphrase[] = "bad-passphrase";
const char kErrorBadWEPKey[] = "bad-wepkey";
const char kErrorConnectFailed[] = "connect-failed";
const char kErrorDNSLookupFailed[] = "dns-lookup-failed";
const char kErrorDhcpFailed[] = "dhcp-failed";
const char kErrorHTTPGetFailed[] = "http-get-failed";
const char kErrorInternal[] = "internal-error";
const char kErrorIpsecCertAuthFailed[] = "ipsec-cert-auth-failed";
const char kErrorIpsecPskAuthFailed[] = "ipsec-psk-auth-failed";
const char kErrorNeedEvdo[] = "need-evdo";
const char kErrorNeedHomeNetwork[] = "need-home-network";
const char kErrorOtaspFailed[] = "otasp-failed";
const char kErrorOutOfRange[] = "out-of-range";
const char kErrorPinMissing[] = "pin-missing";
const char kErrorPppAuthFailed[] = "ppp-auth-failed";

// Flimflam error messages.
const char kErrorPassphraseRequiredMsg[] = "Passphrase required";
const char kErrorIncorrectPinMsg[] = "org.chromium.flimflam.Error.IncorrectPin";
const char kErrorPinBlockedMsg[] = "org.chromium.flimflam.Error.PinBlocked";
const char kErrorPinRequiredMsg[] = "org.chromium.flimflam.Error.PinRequired";

const char kUnknownString[] = "UNKNOWN";
}  // namespace flimflam

namespace shill {
// Manager property names.
const char kHostNameProperty[] = "HostName";
const char kPortalCheckIntervalProperty[] = "PortalCheckInterval";

// Service property names.
const char kHTTPProxyPortProperty[] = "HTTPProxyPort";
const char kIPConfigProperty[] = "IPConfig";
const char kPhysicalTechnologyProperty[] = "PhysicalTechnology";
}  // namespace shill

namespace cashew {
// Cashew D-Bus service identifiers.
const char kCashewServiceName[] = "org.chromium.Cashew";
const char kCashewServicePath[] = "/org/chromium/Cashew";
const char kCashewServiceInterface[] = "org.chromium.Cashew";

// Cashew function names.
const char kRequestDataPlanFunction[] = "RequestDataPlansUpdate";
const char kRetrieveDataPlanFunction[] = "GetDataPlans";
const char kRequestCellularUsageFunction[] = "RequestCellularUsageInfo";

// Cashew signals.
const char kMonitorDataPlanUpdate[] = "DataPlansUpdate";

// Cashew data plan properties
const char kCellularPlanNameProperty[] = "CellularPlanName";
const char kCellularPlanTypeProperty[] = "CellularPlanType";
const char kCellularPlanUpdateTimeProperty[] = "CellularPlanUpdateTime";
const char kCellularPlanStartProperty[] = "CellularPlanStart";
const char kCellularPlanEndProperty[] = "CellularPlanEnd";
const char kCellularPlanDataBytesProperty[] = "CellularPlanDataBytes";
const char kCellularDataBytesUsedProperty[] = "CellularDataBytesUsed";

// Cashew Data Plan types
const char kCellularDataPlanUnlimited[] = "UNLIMITED";
const char kCellularDataPlanMeteredPaid[] = "METERED_PAID";
const char kCellularDataPlanMeteredBase[] = "METERED_BASE";
}  // namespace cashew

namespace modemmanager {
// ModemManager D-Bus service identifiers
const char kModemManagerSMSInterface[] =
    "org.freedesktop.ModemManager.Modem.Gsm.SMS";

// ModemManager function names.
const char kSMSGetFunction[] = "Get";
const char kSMSDeleteFunction[] = "Delete";
const char kSMSListFunction[] = "List";

// ModemManager monitored signals
const char kSMSReceivedSignal[] = "SmsReceived";

// ModemManager1 interfaces and signals
// The canonical source for these is /usr/include/mm/ModemManager-names.h
const char kModemManager1[] = "org.freedesktop.ModemManager1";
const char kModemManager1ServicePath[] = "/org/freedesktop/ModemManager1";
const char kModemManager1MessagingInterface[] =
    "org.freedesktop.ModemManager1.Modem.Messaging";
const char kModemManager1SmsInterface[] =
    "org.freedesktop.ModemManager1.Sms";
const char kSMSAddedSignal[] = "Added";
}  // namespace modemmanager

namespace wimax_manager {
// WiMaxManager D-Bus service identifiers
const char kWiMaxManagerServiceName[] = "org.chromium.WiMaxManager";
const char kWiMaxManagerServicePath[] = "/org/chromium/WiMaxManager";
const char kWiMaxManagerServiceError[] = "org.chromium.WiMaxManager.Error";
const char kWiMaxManagerInterface[] = "org.chromium.WiMaxManager";
const char kDeviceObjectPathPrefix[] = "/org/chromium/WiMaxManager/Device/";
const char kNetworkObjectPathPrefix[] = "/org/chromium/WiMaxManager/Network/";
const char kDevicesProperty[] = "Devices";
const char kNetworksProperty[] = "Networks";
}  // namespace wimax_manager

namespace bluetooth_common {
const char kGetProperties[] = "GetProperties";
const char kSetProperty[] = "SetProperty";
const char kPropertyChangedSignal[] = "PropertyChanged";
}

namespace bluetooth_manager {
// Bluetooth Manager service identifiers.
const char kBluetoothManagerServiceName[] = "org.bluez";
const char kBluetoothManagerServicePath[] = "/";  // crosbug.com/20135
const char kBluetoothManagerInterface[] = "org.bluez.Manager";

// Bluetooth Manager methods.
const char kGetProperties[] = "GetProperties";
const char kDefaultAdapter[] = "DefaultAdapter";
const char kFindAdapter[] = "FindAdapter";

// Bluetooth Manager signals.
const char kPropertyChangedSignal[] = "PropertyChanged";
const char kAdapterAddedSignal[] = "AdapterAdded";
const char kAdapterRemovedSignal[] = "AdapterRemoved";
const char kDefaultAdapterChangedSignal[] = "DefaultAdapterChanged";

// Bluetooth Manager properties.
const char kAdaptersProperty[] = "Adapters";
}  // namespace bluetooth_manager

namespace bluetooth_adapter {
// Bluetooth Adapter service identifiers.
const char kBluetoothAdapterServiceName[] = "org.bluez";
const char kBluetoothAdapterInterface[] = "org.bluez.Adapter";

// Bluetooth Adapter methods.
const char kGetProperties[] = "GetProperties";
const char kSetProperty[] = "SetProperty";
const char kRequestSession[] = "RequestSession";
const char kReleaseSession[] = "ReleaseSession";
const char kStartDiscovery[] = "StartDiscovery";
const char kStopDiscovery[] = "StopDiscovery";
const char kFindDevice[] = "FindDevice";
const char kCreateDevice[] = "CreateDevice";
const char kCreatePairedDevice[] = "CreatePairedDevice";
const char kCancelDeviceCreation[] = "CancelDeviceCreation";
const char kRemoveDevice[] = "RemoveDevice";
const char kRegisterAgent[] = "RegisterAgent";
const char kUnregisterAgent[] = "UnregisterAgent";

// Bluetooth Adapter signals.
const char kPropertyChangedSignal[] = "PropertyChanged";
const char kDeviceFoundSignal[] = "DeviceFound";
const char kDeviceDisappearedSignal[] = "DeviceDisappeared";
const char kDeviceCreatedSignal[] = "DeviceCreated";
const char kDeviceRemovedSignal[] = "DeviceRemoved";

// Bluetooth Adapter properties.
const char kAddressProperty[] = "Address";
const char kNameProperty[] = "Name";
const char kClassProperty[] = "Class";
const char kPoweredProperty[] = "Powered";
const char kDiscoverableProperty[] = "Discoverable";
const char kPairableProperty[] = "Pairable";
const char kPairableTimeoutProperty[] = "PairableTimeout";
const char kDiscoverableTimeoutProperty[] = "DiscoverableTimeout";
const char kDiscoveringProperty[] = "Discovering";
const char kDevicesProperty[] = "Devices";
const char kUUIDsProperty[] = "UUIDs";
}  // namespace bluetooth_adapter

namespace bluetooth_agent {
// Bluetooth Agent service indentifiers
const char kBluetoothAgentInterface[] = "org.bluez.Agent";

// Bluetooth Agent methods.
const char kRelease[] = "Release";
const char kRequestPinCode[] = "RequestPinCode";
const char kRequestPasskey[] = "RequestPasskey";
const char kDisplayPinCode[] = "DisplayPinCode";
const char kDisplayPasskey[] = "DisplayPasskey";
const char kRequestConfirmation[] = "RequestConfirmation";
const char kAuthorize[] = "Authorize";
const char kConfirmModeChange[] = "ConfirmModeChange";
const char kCancel[] = "Cancel";

// Bluetooth Agent capabilities.
const char kNoInputNoOutputCapability[] = "NoInputNoOutput";
const char kDisplayOnlyCapability[] = "DisplayOnly";
const char kKeyboardOnlyCapability[] = "KeyboardOnly";
const char kDisplayYesNoCapability[] = "DisplayYesNo";

// Bluetooth Agent errors.
const char kErrorRejected[] = "org.bluez.Error.Rejected";
const char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_agent

namespace bluetooth_device {
// Bluetooth Device service identifiers.
const char kBluetoothDeviceServiceName[] = "org.bluez";
const char kBluetoothDeviceInterface[] = "org.bluez.Device";

// Bluetooth Device methods.
const char kGetProperties[] = "GetProperties";
const char kSetProperty[] = "SetProperty";
const char kDiscoverServices[] = "DiscoverServices";
const char kCancelDiscovery[] = "CancelDiscovery";
const char kDisconnect[] = "Disconnect";
const char kListNodes[] = "ListNodes";
const char kCreateNode[] = "CreateNode";
const char kRemoveNode[] = "RemoveNode";

// Bluetooth Device signals.
const char kPropertyChangedSignal[] = "PropertyChanged";
const char kDisconnectRequestedSignal[] = "DisconnectRequested";
const char kNodeCreatedSignal[] = "NodeCreated";
const char kNodeRemovedSignal[] = "NodeRemoved";

// Bluetooth Device properties.
const char kAddressProperty[] = "Address";
const char kNameProperty[] = "Name";
const char kVendorProperty[] = "Vendor";
const char kProductProperty[] = "Product";
const char kVersionProperty[] = "Version";
const char kIconProperty[] = "Icon";
const char kClassProperty[] = "Class";
const char kUUIDsProperty[] = "UUIDs";
const char kServicesProperty[] = "Services";
const char kPairedProperty[] = "Paired";
const char kConnectedProperty[] = "Connected";
const char kTrustedProperty[] = "Trusted";
const char kBlockedProperty[] = "Blocked";
const char kAliasProperty[] = "Alias";
const char kNodesProperty[] = "Nodes";
const char kAdapterProperty[] = "Adapter";
const char kLegacyPairingProperty[] = "LegacyPairing";
}  // namespace bluetooth_device

namespace bluetooth_node {
// Bluetooth Node service identifiers.
const char kBluetoothNodeServiceName[] = "org.bluez";
const char kBluetoothNodeInterface[] = "org.bluez.Node";

// Bluetooth Node methods.
const char kGetProperties[] = "GetProperties";

// Bluetooth Node signals.
const char kPropertyChangedSignal[] = "PropertyChanged";

// Bluetoooth Node properties.
const char kNameProperty[] = "Name";
const char kDeviceProperty[] = "Device";
}  // namespace bluetooth_node

namespace bluetooth_input {
// Bluetooth Input service identifiers.
const char kBluetoothInputServiceName[] = "org.bluez";
const char kBluetoothInputInterface[] = "org.bluez.Input";

// Bluetooth Input methods.
const char kConnect[] = "Connect";
const char kDisconnect[] = "Disconnect";
const char kGetProperties[] = "GetProperties";

// Bluetooth Input signals.
const char kPropertyChangedSignal[] = "PropertyChanged";

// Bluetoooth Input properties.
const char kConnectedProperty[] = "Connected";
}  // namespace bluetooth_input

namespace cros_disks {
const char kCrosDisksInterface[] = "org.chromium.CrosDisks";
const char kCrosDisksServicePath[] = "/org/chromium/CrosDisks";
const char kCrosDisksServiceName[] = "org.chromium.CrosDisks";
const char kCrosDisksServiceError[] = "org.chromium.CrosDisks.Error";

// Methods.
const char kEnumerateAutoMountableDevices[] = "EnumerateAutoMountableDevices";
const char kFormat[] = "Format";
// TODO(benchan): Deprecate FormatDevice method (crosbug.com/22981)
const char kFormatDevice[] = "FormatDevice";
const char kGetDeviceProperties[] = "GetDeviceProperties";
const char kMount[] = "Mount";
const char kUnmount[] = "Unmount";

// Signals.
const char kDeviceAdded[] = "DeviceAdded";
const char kDeviceScanned[] = "DeviceScanned";
const char kDeviceRemoved[] = "DeviceRemoved";
const char kDiskAdded[] = "DiskAdded";
const char kDiskChanged[] = "DiskChanged";
const char kDiskRemoved[] = "DiskRemoved";
// TODO(benchan): Deprecate FormattingFinished signal (crosbug.com/22981)
const char kFormattingFinished[] = "FormattingFinished";
const char kFormatCompleted[] = "FormatCompleted";
const char kMountCompleted[] = "MountCompleted";

// Properties.
const char kDeviceFile[] = "DeviceFile";
const char kDeviceIsDrive[] = "DeviceIsDrive";
const char kDeviceIsMediaAvailable[] = "DeviceIsMediaAvailable";
const char kDeviceIsMounted[] = "DeviceIsMounted";
const char kDeviceIsOnBootDevice[] = "DeviceIsOnBootDevice";
const char kDeviceIsReadOnly[] = "DeviceIsReadOnly";
const char kDeviceIsVirtual[] = "DeviceIsVirtual";
const char kDeviceMediaType[] = "DeviceMediaType";
const char kDeviceMountPaths[] = "DeviceMountPaths";
const char kDevicePresentationHide[] = "DevicePresentationHide";
const char kDeviceSize[] = "DeviceSize";
const char kDriveIsRotational[] = "DriveIsRotational";
const char kDriveModel[] = "DriveModel";
const char kExperimentalFeaturesEnabled[] = "ExperimentalFeaturesEnabled";
const char kIdLabel[] = "IdLabel";
const char kIdUuid[] = "IdUuid";
const char kNativePath[] = "NativePath";

// Enum values.
// DeviceMediaType enum values are reported through UMA.
// All values but DEVICE_MEDIA_NUM_VALUES should not be changed or removed.
// Additional values can be added but DEVICE_MEDIA_NUM_VALUES should always
// be the last value in the enum.
enum DeviceMediaType {
  DEVICE_MEDIA_UNKNOWN = 0,
  DEVICE_MEDIA_USB = 1,
  DEVICE_MEDIA_SD = 2,
  DEVICE_MEDIA_OPTICAL_DISC = 3,
  DEVICE_MEDIA_MOBILE = 4,
  DEVICE_MEDIA_DVD = 5,
  DEVICE_MEDIA_NUM_VALUES,
};

enum FormatErrorType {
  FORMAT_ERROR_NONE = 0,
  FORMAT_ERROR_UNKNOWN = 1,
  FORMAT_ERROR_INTERNAL = 2,
  FORMAT_ERROR_INVALID_DEVICE_PATH = 3,
  FORMAT_ERROR_DEVICE_BEING_FORMATTED = 4,
  FORMAT_ERROR_UNSUPPORTED_FILESYSTEM = 5,
  FORMAT_ERROR_FORMAT_PROGRAM_NOT_FOUND = 6,
  FORMAT_ERROR_FORMAT_PROGRAM_FAILED = 7,
};

// TODO(benchan): After both Chrome and cros-disks use these enum values,
// make these error values contiguous so that they can be directly reported
// via UMA.
enum MountErrorType {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_UNKNOWN = 1,
  MOUNT_ERROR_INTERNAL = 2,
  MOUNT_ERROR_INVALID_ARGUMENT = 3,
  MOUNT_ERROR_INVALID_PATH = 4,
  MOUNT_ERROR_PATH_ALREADY_MOUNTED = 5,
  MOUNT_ERROR_PATH_NOT_MOUNTED = 6,
  MOUNT_ERROR_DIRECTORY_CREATION_FAILED = 7,
  MOUNT_ERROR_INVALID_MOUNT_OPTIONS = 8,
  MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS = 9,
  MOUNT_ERROR_INSUFFICIENT_PERMISSIONS = 10,
  MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND = 11,
  MOUNT_ERROR_MOUNT_PROGRAM_FAILED = 12,
  MOUNT_ERROR_INVALID_DEVICE_PATH = 100,
  MOUNT_ERROR_UNKNOWN_FILESYSTEM = 101,
  MOUNT_ERROR_UNSUPPORTED_FILESYSTEM = 102,
  MOUNT_ERROR_INVALID_ARCHIVE = 201,
  MOUNT_ERROR_UNSUPPORTED_ARCHIVE = 202,
};

// MountSourceType enum values are solely used by Chrome/CrosDisks in
// the MountCompleted signal, and currently not reported through UMA.
enum MountSourceType {
  MOUNT_SOURCE_INVALID = 0,
  MOUNT_SOURCE_REMOVABLE_DEVICE = 1,
  MOUNT_SOURCE_ARCHIVE = 2,
  MOUNT_SOURCE_NETWORK_STORAGE = 3,
};
}  // namespace cros_disks

namespace update_engine {
const char kUpdateEngineInterface[] = "org.chromium.UpdateEngineInterface";
const char kUpdateEngineServicePath[] = "/org/chromium/UpdateEngine";
const char kUpdateEngineServiceName[] = "org.chromium.UpdateEngine";

// Methods.
const char kAttemptUpdate[] = "AttemptUpdate";
const char kGetStatus[] = "GetStatus";
const char kGetTrack[] = "GetTrack";
const char kRebootIfNeeded[] = "RebootIfNeeded";
const char kSetTrack[] = "SetTrack";

// Signals.
const char kStatusUpdate[] = "StatusUpdate";
}  // namespace update_engine

namespace debugd {
const char kDebugdInterface[] = "org.chromium.debugd";
const char kDebugdServicePath[] = "/org/chromium/debugd";
const char kDebugdServiceName[] = "org.chromium.debugd";

// Methods.
const char kGetDebugLogs[] = "GetDebugLogs";
const char kGetModemStatus[] = "GetModemStatus";
const char kGetNetworkStatus[] = "GetNetworkStatus";
const char kGetRoutes[] = "GetRoutes";
const char kSetDebugMode[] = "SetDebugMode";
const char kSystraceStart[] = "SystraceStart";
const char kSystraceStop[] = "SystraceStop";
const char kSytraceStatus[] = "SystraceStatus";
}  // namespace debugd

#endif  // DBUS_SERVICE_CONSTANTS_H_
