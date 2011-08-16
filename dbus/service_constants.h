// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SERVICE_CONSTANTS_H_
#define CHROMEOS_DBUS_SERVICE_CONSTANTS_H_

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
const char kCryptohomePkcs11IsTpmTokenReady[] = "Pkcs11IsTpmTokenReady";
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
const char kCryptohomeAsyncSetOwnerUser[] = "AsyncSetOwnerUser";
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
}  // namespace cryptohome

namespace imageburn {
const char kImageBurnServiceName[] = "org.chromium.ImageBurner";
const char kImageBurnServicePath[] = "/org/chromium/ImageBurner";
const char kImageBurnServiceInterface[] = "org.chromium.ImageBurnerInterface";
//Methods
const char kBurnImage[] = "BurnImage";
//Signals
const char kSignalBurnFinishedName[] = "burn_finished";
const char kSignalBurnUpdateName[] = "burn_progress_update";
} // namespace imageburn

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
// Signals
const char kSessionManagerSessionStateChanged[] = "SessionStateChanged";
}  // namespace login_manager

namespace speech_synthesis {
const char kSpeechSynthesizerInterface[] =
    "org.chromium.SpeechSynthesizerInterface";
const char kSpeechSynthesizerServicePath[] = "/org/chromium/SpeechSynthesizer";
const char kSpeechSynthesizerServiceName[] = "org.chromium.SpeechSynthesizer";
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
const char kPowerManagerDecreaseScreenBrightness[] =
    "DecreaseScreenBrightness";
const char kPowerManagerIncreaseScreenBrightness[] =
    "IncreaseScreenBrightness";
const char kPowerManagerDecreaseKeyboardBrightness[] =
    "DecreaseKeyboardBrightness";
const char kPowerManagerIncreaseKeyboardBrightness[] =
    "IncreaseKeyboardBrightness";
// Signals
const char kRequestLockScreenSignal[] = "RequestLockScreen";
const char kRequestRestartSignal[] = "RequestRestart";
const char kRequestSuspendSignal[] = "RequestSuspend";
const char kRequestShutdownSignal[] = "RequestShutdown";
const char kRequestUnlockScreenSignal[] = "RequestUnlockScreen";
const char kScreenIsLockedSignal[] = "ScreenIsLocked";
const char kScreenIsUnlockedSignal[] = "ScreenIsUnlocked";
const char kCleanShutdown[] = "CleanShutdown";
const char kRegisterSuspendDelay[] = "RegisterSuspendDelay";
const char kUnregisterSuspendDelay[] = "UnregisterSuspendDelay";
const char kSuspendDelay[] = "SuspendDelay";
const char kSuspendReady[] = "SuspendReady";
const char kBrightnessChangedSignal[] = "BrightnessChanged";
const char kKeyboardBrightnessChangedSignal[] = "KeyboardBrightnessChanged";
const char kPowerStateChangedSignal[] = "PowerStateChanged";
}  // namespace power_manager

namespace chromeos {
const char kLibCrosServiceName[] = "org.chromium.LibCrosService";
const char kLibCrosServicePath[] = "/org/chromium/LibCrosService";
const char kLibCrosServiceInterface[] = "org.chromium.LibCrosServiceInterface";
// Methods
const char kResolveNetworkProxy[] = "ResolveNetworkProxy";
} // namespace chromeos

namespace flimflam {
// Flimflam D-Bus service identifiers.
const char kFlimflamManagerInterface[] = "org.chromium.flimflam.Manager";
const char kFlimflamServiceInterface[] = "org.chromium.flimflam.Service";
const char kFlimflamServiceName[] = "org.chromium.flimflam";
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
const char kGetWifiServiceFunction[] = "GetWifiService";
const char kGetVPNServiceFunction[] = "GetVPNService";
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

// Flimflam Wifi Service property names.
const char kWifiHexSsid[] = "WiFi.HexSSID";
const char kWifiFrequency[] = "WiFi.Frequency";
const char kWifiHiddenSsid[] = "WiFi.HiddenSSID";
const char kWifiPhyMode[] = "WiFi.PhyMode";
const char kWifiAuthMode[] = "WiFi.AuthMode";
const char kWifiChannelProperty[] = "WiFi.Channel";

// Flimflam EAP property names.
const char kEapIdentityProperty[] = "EAP.Identity";
const char kEAPEAPProperty[] = "EAP.EAP";
const char kEapPhase2AuthProperty[] = "EAP.InnerEAP";
const char kEapAnonymousIdentityProperty[] = "EAP.AnonymousIdentity";
const char kEAPClientCertProperty[] = "EAP.ClientCert";
const char kEAPCertIDProperty[] = "EAP.CertID";
const char kEapClientCertNssProperty[] = "EAP.ClientCertNSS";
const char kEapPrivateKeyProperty[] = "EAP.PrivateKey";
const char kEapPrivateKeyPasswordProperty[] = "EAP.PrivateKeyPassword";
const char kEAPKeyIDProperty[] = "EAP.KeyID";
const char kEapCaCertProperty[] = "EAP.CACert";
const char kEapCaCertIDProperty[] = "EAP.CACertID";
const char kEapCaCertNssProperty[] = "EAP.CACertNSS";
const char kEapUseSystemCAsProperty[] = "EAP.UseSystemCAs";
const char kEAPPINProperty[] = "EAP.PIN";
const char kEapPasswordProperty[] = "EAP.Password";
const char kEapKeyMgmtProperty[] = "EAP.KeyMgmt";

// Flimflam Cellular Service property names.
const char kActivationStateProperty[] = "Cellular.ActivationState";
const char kNetworkTechnologyProperty[] = "Cellular.NetworkTechnology";
const char kRoamingStateProperty[] = "Cellular.RoamingState";
const char kOperatorNameProperty[] = "Cellular.OperatorName";
const char kOperatorCodeProperty[] = "Cellular.OperatorCode";
const char kServingOperatorProperty[] = "Cellular.ServingOperator";
const char kPaymentURLProperty[] = "Cellular.OlpUrl";
const char kUsageURLProperty[] = "Cellular.UsageUrl";
const char kCellularApnProperty[] = "Cellular.APN";
const char kCellularLastGoodApnProperty[] = "Cellular.LastGoodAPN";

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

// Flimflam property names for Cellular.FoundNetworks.
const char kLongNameProperty[] = "long_name";
const char kStatusProperty[] = "status";
const char kShortNameProperty[] = "short_name";
const char kTechnologyProperty[] = "technology";

// Flimflam SIMLock status types.
const char kSIMLockPin[] = "sim-pin";
const char kSIMLockPuk[] = "sim-puk";

// APN info property names.
const char kApnProperty[] = "apn";
const char kNetworkIdProperty[] = "network_id";
const char kUsernameProperty[] = "username";
const char kPasswordProperty[] = "password";

// Operator info property names.
const char kOperatorNameKey[] = "name";
const char kOperatorCodeKey[] = "code";
const char kOperatorCountryKey[] = "country";

// Flimflam network technology options.
const char kNetworkTechnology1Xrtt[] = "1xRTT";
const char kNetworkTechnologyEvdo[] = "EVDO";
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
const char kEapPhase2AuthTTLSMD5[] = "autheap=MD5";
const char kEapPhase2AuthTTLSMSCHAPV2[] = "autheap=MSCHAPV2";
const char kEapPhase2AuthTTLSMSCHAP[] = "autheap=MSCHAP";
const char kEapPhase2AuthTTLSPAP[] = "autheap=PAP";
const char kEapPhase2AuthTTLSCHAP[] = "autheap=CHAP";

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
const char kL2TPIPSecCACertNSSProperty[] = "L2TPIPsec.CACertNSS";
const char kL2TPIPSecClientCertIDProperty[] = "L2TPIPsec.ClientCertID";
const char kL2TPIPSecPSKProperty[] = "L2TPIPsec.PSK";
const char kL2TPIPSecUserProperty[] = "L2TPIPsec.User";
const char kL2TPIPSecPasswordProperty[] = "L2TPIPsec.Password";

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
const char kErrorOutOfRange[] = "out-of-range";
const char kErrorPinMissing[] = "pin-missing";
const char kErrorDhcpFailed[] = "dhcp-failed";
const char kErrorConnectFailed[] = "connect-failed";
const char kErrorBadPassphrase[] = "bad-passphrase";
const char kErrorBadWEPKey[] = "bad-wepkey";
const char kErrorActivationFailed[] = "activation-failed";
const char kErrorNeedEvdo[] = "need-evdo";
const char kErrorNeedHomeNetwork[] = "need-home-network";
const char kErrorOtaspFailed[] = "otasp-failed";
const char kErrorAaaFailed[] = "aaa-failed";
const char kErrorInternal[] = "internal-error";
const char kErrorDNSLookupFailed[] = "dns-lookup-failed";
const char kErrorHTTPGetFailed[] = "http-get-failed";

// Flimflam error messages.
const char kErrorPassphraseRequiredMsg[] = "Passphrase required";
const char kErrorIncorrectPinMsg[] =
    "org.chromium.flimflam.Error.IncorrectPin";
const char kErrorPinBlockedMsg[] = "org.chromium.flimflam.Error.PinBlocked";
const char kErrorPinRequiredMsg[] = "org.chromium.flimflam.Error.PinRequired";

const char kUnknownString[] = "UNKNOWN";
}  // namespace flimflam

namespace cashew {
// Cashew D-Bus service identifiers.
const char kCashewServiceName[] = "org.chromium.Cashew";
const char kCashewServicePath[] = "/org/chromium/Cashew";
const char kCashewServiceInterface[] = "org.chromium.Cashew";

// Cashew function names.
const char kRequestDataPlanFunction[] = "RequestDataPlansUpdate";
const char kRetrieveDataPlanFunction[] = "GetDataPlans";

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
}  // namespace modemmanager

#endif  // CHROMEOS_DBUS_SERVICE_CONSTANTS_H_
