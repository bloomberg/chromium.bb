// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_BLUETOOTH_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_BLUETOOTH_DBUS_CONSTANTS_H_

namespace bluetooth_plugin {
// Service identifiers for the plugin interface added to the /org/bluez object.
constexpr char kBluetoothPluginServiceName[] = "org.bluez";
constexpr char kBluetoothPluginInterface[] = "org.chromium.Bluetooth";

// Bluetooth plugin properties.
constexpr char kSupportsLEServices[] = "SupportsLEServices";
constexpr char kSupportsConnInfo[] = "SupportsConnInfo";
}  // namespace bluetooth_plugin

namespace bluetooth_plugin_device {
// Service identifiers for the plugin interface added to Bluetooth Device
// objects.
constexpr char kBluetoothPluginServiceName[] = "org.bluez";
constexpr char kBluetoothPluginInterface[] = "org.chromium.BluetoothDevice";

// Bluetooth Device plugin methods.
constexpr char kGetConnInfo[] = "GetConnInfo";
constexpr char kSetLEConnectionParameters[] = "SetLEConnectionParameters";
// Valid connection parameters that can be passed to the
// SetLEConnectionParameters API as dictionary keys.
constexpr char kLEConnectionParameterMinimumConnectionInterval[] =
    "MinimumConnectionInterval";
constexpr char kLEConnectionParameterMaximumConnectionInterval[] =
    "MaximumConnectionInterval";
}  // namespace bluetooth_plugin_device

namespace bluetooth_adapter {
// Bluetooth Adapter service identifiers.
constexpr char kBluetoothAdapterServiceName[] = "org.bluez";
constexpr char kBluetoothAdapterInterface[] = "org.bluez.Adapter1";

// Bluetooth Adapter methods.
constexpr char kStartDiscovery[] = "StartDiscovery";
constexpr char kSetDiscoveryFilter[] = "SetDiscoveryFilter";
constexpr char kStopDiscovery[] = "StopDiscovery";
constexpr char kPauseDiscovery[] = "PauseDiscovery";
constexpr char kUnpauseDiscovery[] = "UnpauseDiscovery";
constexpr char kRemoveDevice[] = "RemoveDevice";
constexpr char kCreateServiceRecord[] = "CreateServiceRecord";
constexpr char kRemoveServiceRecord[] = "RemoveServiceRecord";
constexpr char kHandleSuspendImminent[] = "HandleSuspendImminent";
constexpr char kHandleSuspendDone[] = "HandleSuspendDone";
constexpr char kGetSupportedCapabilities[] = "GetSupportedCapabilities";
constexpr char kSetLongTermKeys[] = "SetLongTermKeys";

// Bluetooth Adapter properties.
constexpr char kAddressProperty[] = "Address";
constexpr char kNameProperty[] = "Name";
constexpr char kAliasProperty[] = "Alias";
constexpr char kClassProperty[] = "Class";
constexpr char kPoweredProperty[] = "Powered";
constexpr char kDiscoverableProperty[] = "Discoverable";
constexpr char kPairableProperty[] = "Pairable";
constexpr char kPairableTimeoutProperty[] = "PairableTimeout";
constexpr char kDiscoverableTimeoutProperty[] = "DiscoverableTimeout";
constexpr char kDiscoveringProperty[] = "Discovering";
constexpr char kUUIDsProperty[] = "UUIDs";
constexpr char kModaliasProperty[] = "Modalias";
constexpr char kStackSyncQuittingProperty[] = "StackSyncQuitting";
constexpr char kUseSuspendNotifierProperty[] = "UseSuspendNotifier";

// Bluetooth Adapter errors.
constexpr char kErrorNotReady[] = "org.bluez.Error.NotReady";
constexpr char kErrorFailed[] = "org.bluez.Error.Failed";
constexpr char kErrorInProgress[] = "org.bluez.Error.InProgress";
constexpr char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
constexpr char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
constexpr char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
constexpr char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";

// Bluetooth Adapter parameters supplied to SetDiscoveryFilter request.
constexpr char kDiscoveryFilterParameterUUIDs[] = "UUIDs";
constexpr char kDiscoveryFilterParameterRSSI[] = "RSSI";
constexpr char kDiscoveryFilterParameterPathloss[] = "Pathloss";
constexpr char kDiscoveryFilterParameterTransport[] = "Transport";
}  // namespace bluetooth_adapter

namespace bluetooth_agent_manager {
// Bluetooth Agent Manager service indentifiers
constexpr char kBluetoothAgentManagerServiceName[] = "org.bluez";
constexpr char kBluetoothAgentManagerServicePath[] = "/org/bluez";
constexpr char kBluetoothAgentManagerInterface[] = "org.bluez.AgentManager1";

// Bluetooth Agent Manager methods.
constexpr char kRegisterAgent[] = "RegisterAgent";
constexpr char kUnregisterAgent[] = "UnregisterAgent";
constexpr char kRequestDefaultAgent[] = "RequestDefaultAgent";

// Bluetooth capabilities.
constexpr char kNoInputNoOutputCapability[] = "NoInputNoOutput";
constexpr char kDisplayOnlyCapability[] = "DisplayOnly";
constexpr char kKeyboardOnlyCapability[] = "KeyboardOnly";
constexpr char kDisplayYesNoCapability[] = "DisplayYesNo";
constexpr char kKeyboardDisplayCapability[] = "KeyboardDisplay";

// Bluetooth Agent Manager errors.
constexpr char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
constexpr char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
constexpr char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_agent_manager

namespace bluetooth_agent {
// Bluetooth Agent service indentifiers
constexpr char kBluetoothAgentInterface[] = "org.bluez.Agent1";

// Bluetooth Agent methods.
constexpr char kRelease[] = "Release";
constexpr char kRequestPinCode[] = "RequestPinCode";
constexpr char kDisplayPinCode[] = "DisplayPinCode";
constexpr char kRequestPasskey[] = "RequestPasskey";
constexpr char kDisplayPasskey[] = "DisplayPasskey";
constexpr char kRequestConfirmation[] = "RequestConfirmation";
constexpr char kRequestAuthorization[] = "RequestAuthorization";
constexpr char kAuthorizeService[] = "AuthorizeService";
constexpr char kCancel[] = "Cancel";

// Bluetooth Agent errors.
constexpr char kErrorRejected[] = "org.bluez.Error.Rejected";
constexpr char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_agent

namespace bluetooth_device {
// Bluetooth Device service identifiers.
constexpr char kBluetoothDeviceServiceName[] = "org.bluez";
constexpr char kBluetoothDeviceInterface[] = "org.bluez.Device1";

// Bluetooth Device methods.
constexpr char kConnect[] = "Connect";
constexpr char kDisconnect[] = "Disconnect";
constexpr char kConnectProfile[] = "ConnectProfile";
constexpr char kDisconnectProfile[] = "DisconnectProfile";
constexpr char kPair[] = "Pair";
constexpr char kCancelPairing[] = "CancelPairing";
constexpr char kGetServiceRecords[] = "GetServiceRecords";
constexpr char kExecuteWrite[] = "ExecuteWrite";

// Bluetooth Device properties.
constexpr char kAddressProperty[] = "Address";
constexpr char kNameProperty[] = "Name";
constexpr char kIconProperty[] = "Icon";
constexpr char kClassProperty[] = "Class";
constexpr char kTypeProperty[] = "Type";
constexpr char kAppearanceProperty[] = "Appearance";
constexpr char kUUIDsProperty[] = "UUIDs";
constexpr char kPairedProperty[] = "Paired";
constexpr char kConnectedProperty[] = "Connected";
constexpr char kTrustedProperty[] = "Trusted";
constexpr char kBlockedProperty[] = "Blocked";
constexpr char kAliasProperty[] = "Alias";
constexpr char kAdapterProperty[] = "Adapter";
constexpr char kLegacyPairingProperty[] = "LegacyPairing";
constexpr char kModaliasProperty[] = "Modalias";
constexpr char kRSSIProperty[] = "RSSI";
constexpr char kTxPowerProperty[] = "TxPower";
constexpr char kManufacturerDataProperty[] = "ManufacturerData";
constexpr char kServiceDataProperty[] = "ServiceData";
constexpr char kServicesResolvedProperty[] = "ServicesResolved";
constexpr char kAdvertisingDataFlagsProperty[] = "AdvertisingFlags";
constexpr char kMTUProperty[] = "MTU";
constexpr char kEIRProperty[] = "EIR";

// Bluetooth Device errors.
constexpr char kErrorNotReady[] = "org.bluez.Error.NotReady";
constexpr char kErrorFailed[] = "org.bluez.Error.Failed";
constexpr char kErrorInProgress[] = "org.bluez.Error.InProgress";
constexpr char kErrorAlreadyConnected[] = "org.bluez.Error.AlreadyConnected";
constexpr char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
constexpr char kErrorNotConnected[] = "org.bluez.Error.NotConnected";
constexpr char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
constexpr char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";

// Undocumented errors that we know BlueZ returns for Bluetooth Device methods.
constexpr char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
constexpr char kErrorAuthenticationCanceled[] =
    "org.bluez.Error.AuthenticationCanceled";
constexpr char kErrorAuthenticationFailed[] =
    "org.bluez.Error.AuthenticationFailed";
constexpr char kErrorAuthenticationRejected[] =
    "org.bluez.Error.AuthenticationRejected";
constexpr char kErrorAuthenticationTimeout[] =
    "org.bluez.Error.AuthenticationTimeout";
constexpr char kErrorConnectionAttemptFailed[] =
    "org.bluez.Error.ConnectionAttemptFailed";
}  // namespace bluetooth_device

namespace bluetooth_gatt_characteristic {
// Bluetooth GATT Characteristic service identifiers. The service name is used
// only for characteristic objects hosted by bluetoothd.
constexpr char kBluetoothGattCharacteristicServiceName[] = "org.bluez";
constexpr char kBluetoothGattCharacteristicInterface[] =
    "org.bluez.GattCharacteristic1";

// Bluetooth GATT Characteristic methods.
constexpr char kReadValue[] = "ReadValue";
constexpr char kWriteValue[] = "WriteValue";
constexpr char kStartNotify[] = "StartNotify";
constexpr char kStopNotify[] = "StopNotify";
constexpr char kPrepareWriteValue[] = "PrepareWriteValue";

// Bluetooth GATT Characteristic signals.
constexpr char kValueUpdatedSignal[] = "ValueUpdated";

// Possible keys for option dict used in ReadValue, WriteValue and
// PrepareWriteValue.
constexpr char kOptionOffset[] = "offset";
constexpr char kOptionDevice[] = "device";
constexpr char kOptionHasSubsequentWrite[] = "has-subsequent-write";

// Bluetooth GATT Characteristic properties.
constexpr char kUUIDProperty[] = "UUID";
constexpr char kServiceProperty[] = "Service";
constexpr char kValueProperty[] = "Value";
constexpr char kFlagsProperty[] = "Flags";
constexpr char kNotifyingProperty[] = "Notifying";
constexpr char kDescriptorsProperty[] = "Descriptors";

// Possible values for Bluetooth GATT Characteristic "Flags" property.
constexpr char kFlagBroadcast[] = "broadcast";
constexpr char kFlagRead[] = "read";
constexpr char kFlagWriteWithoutResponse[] = "write-without-response";
constexpr char kFlagWrite[] = "write";
constexpr char kFlagNotify[] = "notify";
constexpr char kFlagIndicate[] = "indicate";
constexpr char kFlagAuthenticatedSignedWrites[] = "authenticated-signed-writes";
constexpr char kFlagExtendedProperties[] = "extended-properties";
constexpr char kFlagReliableWrite[] = "reliable-write";
constexpr char kFlagWritableAuxiliaries[] = "writable-auxiliaries";
constexpr char kFlagEncryptRead[] = "encrypt-read";
constexpr char kFlagEncryptWrite[] = "encrypt-write";
constexpr char kFlagEncryptAuthenticatedRead[] = "encrypt-authenticated-read";
constexpr char kFlagEncryptAuthenticatedWrite[] = "encrypt-authenticated-write";
constexpr char kFlagPermissionRead[] = "permission-read";
constexpr char kFlagPermissionWrite[] = "permission-write";
constexpr char kFlagPermissionEncryptRead[] = "permission-encrypt-read";
constexpr char kFlagPermissionEncryptWrite[] = "permission-encrypt-write";
constexpr char kFlagPermissionAuthenticatedRead[] =
    "permission-authenticated-read";
constexpr char kFlagPermissionAuthenticatedWrite[] =
    "permission-authenticated-write";
constexpr char kFlagPermissionSecureRead[] = "permission-secure-read";
constexpr char kFlagPermissionSecureWrite[] = "permission-secure-write";

// Bluetooth GATT Characteristic errors.
constexpr char kErrorFailed[] = "org.bluez.Error.Failed";
constexpr char kErrorInProgress[] = "org.bluez.Error.InProgress";
constexpr char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
constexpr char kErrorInvalidValueLength[] =
    "org.bluez.Error.InvalidValueLength";
constexpr char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
constexpr char kErrorNotConnected[] = "org.bluez.Error.NotConnected";
constexpr char kErrorNotPermitted[] = "org.bluez.Error.NotPermitted";
constexpr char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
}  // namespace bluetooth_gatt_characteristic

namespace bluetooth_gatt_descriptor {
// Bluetooth GATT Descriptor service identifiers. The service name is used
// only for descriptor objects hosted by bluetoothd.
constexpr char kBluetoothGattDescriptorServiceName[] = "org.bluez";
constexpr char kBluetoothGattDescriptorInterface[] =
    "org.bluez.GattDescriptor1";

// Bluetooth GATT Descriptor methods.
constexpr char kReadValue[] = "ReadValue";
constexpr char kWriteValue[] = "WriteValue";

// Possible keys for option dict used in ReadValue and WriteValue.
constexpr char kOptionOffset[] = "offset";
constexpr char kOptionDevice[] = "device";

// Bluetooth GATT Descriptor properties.
constexpr char kUUIDProperty[] = "UUID";
constexpr char kCharacteristicProperty[] = "Characteristic";
constexpr char kValueProperty[] = "Value";
constexpr char kFlagsProperty[] = "Flags";

// Possible values for Bluetooth GATT Descriptor "Flags" property.
constexpr char kFlagRead[] = "read";
constexpr char kFlagWrite[] = "write";
constexpr char kFlagEncryptRead[] = "encrypt-read";
constexpr char kFlagEncryptWrite[] = "encrypt-write";
constexpr char kFlagEncryptAuthenticatedRead[] = "encrypt-authenticated-read";
constexpr char kFlagEncryptAuthenticatedWrite[] = "encrypt-authenticated-write";

// Bluetooth GATT Descriptor errors.
constexpr char kErrorFailed[] = "org.bluez.Error.Failed";
constexpr char kErrorInProgress[] = "org.bluez.Error.InProgress";
constexpr char kErrorInvalidValueLength[] =
    "org.bluez.Error.InvalidValueLength";
constexpr char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
constexpr char kErrorNotPermitted[] = "org.bluez.Error.NotPermitted";
constexpr char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
}  // namespace bluetooth_gatt_descriptor

namespace bluetooth_gatt_manager {
// Bluetooth GATT Manager service identifiers.
constexpr char kBluetoothGattManagerServiceName[] = "org.bluez";
constexpr char kBluetoothGattManagerInterface[] = "org.bluez.GattManager1";

// Bluetooth GATT Manager methods.
constexpr char kRegisterApplication[] = "RegisterApplication";
constexpr char kUnregisterApplication[] = "UnregisterApplication";
constexpr char kRegisterService[] = "RegisterService";
constexpr char kUnregisterService[] = "UnregisterService";

// Bluetooth GATT Manager errors.
constexpr char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
constexpr char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
constexpr char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_gatt_manager

namespace bluetooth_gatt_service {
// Bluetooth GATT Service service identifiers. The service name is used
// only for service objects hosted by bluetoothd.
constexpr char kBluetoothGattServiceServiceName[] = "org.bluez";
constexpr char kBluetoothGattServiceInterface[] = "org.bluez.GattService1";

// Bluetooth GATT Service properties.
constexpr char kUUIDProperty[] = "UUID";
constexpr char kDeviceProperty[] = "Device";
constexpr char kPrimaryProperty[] = "Primary";
constexpr char kIncludesProperty[] = "Includes";
constexpr char kCharacteristicsProperty[] = "Characteristics";

// Bluetooth GATT Service errors.
constexpr char kErrorFailed[] = "org.bluez.Error.Failed";
constexpr char kErrorInProgress[] = "org.bluez.Error.InProgress";
constexpr char kErrorInvalidValueLength[] =
    "org.bluez.Error.InvalidValueLength";
constexpr char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
constexpr char kErrorNotPaired[] = "org.bluez.Error.NotPaired";
constexpr char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
constexpr char kErrorNotPermitted[] = "org.bluez.Error.NotPermitted";
}  // namespace bluetooth_gatt_service

namespace bluetooth_input {
// Bluetooth Input service identifiers.
constexpr char kBluetoothInputServiceName[] = "org.bluez";
constexpr char kBluetoothInputInterface[] = "org.bluez.Input1";

// Bluetooth Input properties.
constexpr char kReconnectModeProperty[] = "ReconnectMode";

// Bluetooth Input property values.
constexpr char kNoneReconnectModeProperty[] = "none";
constexpr char kHostReconnectModeProperty[] = "host";
constexpr char kDeviceReconnectModeProperty[] = "device";
constexpr char kAnyReconnectModeProperty[] = "any";
}  // namespace bluetooth_input

namespace bluetooth_media {
// Bluetooth Media service identifiers
constexpr char kBluetoothMediaServiceName[] = "org.bluez";
constexpr char kBluetoothMediaInterface[] = "org.bluez.Media1";

// Bluetooth Media methods
constexpr char kRegisterEndpoint[] = "RegisterEndpoint";
constexpr char kUnregisterEndpoint[] = "UnregisterEndpoint";
constexpr char kRegisterPlayer[] = "RegisterPlayer";
constexpr char kUnregisterPlayer[] = "UnregisterPlayer";

// Bluetooth Media errors
constexpr char kErrorFailed[] = "org.bluez.Error.Failed";
constexpr char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
constexpr char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
}  // namespace bluetooth_media

namespace bluetooth_media_endpoint {
// Bluetooth Media Endpoint service identifiers
constexpr char kBluetoothMediaEndpointServiceName[] = "org.bluez";
constexpr char kBluetoothMediaEndpointInterface[] = "org.bluez.MediaEndpoint1";

// Bluetooth Media Endpoint methods
constexpr char kSetConfiguration[] = "SetConfiguration";
constexpr char kSelectConfiguration[] = "SelectConfiguration";
constexpr char kClearConfiguration[] = "ClearConfiguration";
constexpr char kRelease[] = "Release";
}  // namespace bluetooth_media_endpoint

namespace bluetooth_media_transport {
// Bluetooth Media Transport service identifiers
constexpr char kBluetoothMediaTransportServiceName[] = "org.bluez";
constexpr char kBluetoothMediaTransportInterface[] =
    "org.bluez.MediaTransport1";

// Bluetooth Media Transport methods
constexpr char kAcquire[] = "Acquire";
constexpr char kTryAcquire[] = "TryAcquire";
constexpr char kRelease[] = "Release";

// Bluetooth Media Transport property names.
constexpr char kDeviceProperty[] = "Device";
constexpr char kUUIDProperty[] = "UUID";
constexpr char kCodecProperty[] = "Codec";
constexpr char kConfigurationProperty[] = "Configuration";
constexpr char kStateProperty[] = "State";
constexpr char kDelayProperty[] = "Delay";
constexpr char kVolumeProperty[] = "Volume";

// Possible states for the "State" property
constexpr char kStateIdle[] = "idle";
constexpr char kStatePending[] = "pending";
constexpr char kStateActive[] = "active";

// Bluetooth Media Transport errors.
constexpr char kErrorFailed[] = "org.bluez.Error.Failed";
constexpr char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
constexpr char kErrorNotAvailable[] = "org.bluez.Error.NotAvailable";
}  // namespace bluetooth_media_transport

namespace bluez_object_manager {
// BlueZ daemon Object Manager service identifiers.
constexpr char kBluezObjectManagerServiceName[] = "org.bluez";
constexpr char kBluezObjectManagerServicePath[] = "/";
}  // namespace bluez_object_manager

namespace bluetooth_object_manager {
// Bluetooth daemon Object Manager service identifiers.
constexpr char kBluetoothObjectManagerServiceName[] = "org.chromium.Bluetooth";
constexpr char kBluetoothObjectManagerServicePath[] = "/";
}  // namespace bluetooth_object_manager

namespace newblue_object_manager {
// NewBlue daemon Object Manager service identifiers.
constexpr char kNewblueObjectManagerServiceName[] = "org.chromium.Newblue";
constexpr char kNewblueObjectManagerServicePath[] = "/";
}  // namespace newblue_object_manager

namespace bluetooth_profile_manager {
// Bluetooth Profile Manager service identifiers.
constexpr char kBluetoothProfileManagerServiceName[] = "org.bluez";
constexpr char kBluetoothProfileManagerServicePath[] = "/org/bluez";
constexpr char kBluetoothProfileManagerInterface[] =
    "org.bluez.ProfileManager1";

// Bluetooth Profile Manager methods.
constexpr char kRegisterProfile[] = "RegisterProfile";
constexpr char kUnregisterProfile[] = "UnregisterProfile";

// Bluetooth Profile Manager option names.
constexpr char kNameOption[] = "Name";
constexpr char kServiceOption[] = "Service";
constexpr char kRoleOption[] = "Role";
constexpr char kChannelOption[] = "Channel";
constexpr char kPSMOption[] = "PSM";
constexpr char kRequireAuthenticationOption[] = "RequireAuthentication";
constexpr char kRequireAuthorizationOption[] = "RequireAuthorization";
constexpr char kAutoConnectOption[] = "AutoConnect";
constexpr char kServiceRecordOption[] = "ServiceRecord";
constexpr char kVersionOption[] = "Version";
constexpr char kFeaturesOption[] = "Features";

// Bluetooth Profile Manager option values.
constexpr char kClientRoleOption[] = "client";
constexpr char kServerRoleOption[] = "server";

// Bluetooth Profile Manager errors.
constexpr char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
constexpr char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
constexpr char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_profile_manager

namespace bluetooth_profile {
// Bluetooth Profile service identifiers.
constexpr char kBluetoothProfileInterface[] = "org.bluez.Profile1";

// Bluetooth Profile methods.
constexpr char kRelease[] = "Release";
constexpr char kNewConnection[] = "NewConnection";
constexpr char kRequestDisconnection[] = "RequestDisconnection";
constexpr char kCancel[] = "Cancel";

// Bluetooth Profile property names.
constexpr char kVersionProperty[] = "Version";
constexpr char kFeaturesProperty[] = "Features";

// Bluetooth Profile errors.
constexpr char kErrorRejected[] = "org.bluez.Error.Rejected";
constexpr char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_profile

namespace bluetooth_advertisement {
// Bluetooth LE Advertisement service identifiers.
constexpr char kBluetoothAdvertisementServiceName[] = "org.bluez";
constexpr char kBluetoothAdvertisementInterface[] =
    "org.bluez.LEAdvertisement1";

// Bluetooth Advertisement methods.
constexpr char kRelease[] = "Release";

// Bluetooth Advertisement properties.
constexpr char kManufacturerDataProperty[] = "ManufacturerData";
constexpr char kServiceUUIDsProperty[] = "ServiceUUIDs";
constexpr char kServiceDataProperty[] = "ServiceData";
constexpr char kSolicitUUIDsProperty[] = "SolicitUUIDs";
constexpr char kTypeProperty[] = "Type";
constexpr char kIncludeTxPowerProperty[] = "IncludeTxPower";

// Possible values for the "Type" property.
constexpr char kTypeBroadcast[] = "broadcast";
constexpr char kTypePeripheral[] = "peripheral";
}  // namespace bluetooth_advertisement

namespace bluetooth_advertising_manager {
// Bluetooth LE Advertising Manager service identifiers.
constexpr char kBluetoothAdvertisingManagerServiceName[] = "org.bluez";
constexpr char kBluetoothAdvertisingManagerInterface[] =
    "org.bluez.LEAdvertisingManager1";

// Bluetooth LE Advertising Manager methods.
constexpr char kRegisterAdvertisement[] = "RegisterAdvertisement";
constexpr char kUnregisterAdvertisement[] = "UnregisterAdvertisement";
constexpr char kSetAdvertisingIntervals[] = "SetAdvertisingIntervals";
constexpr char kResetAdvertising[] = "ResetAdvertising";

// Bluetooth LE Advertising Manager properties.
constexpr char kIsTXPowerSupportedProperty[] = "IsTXPowerSupported";

// Bluetooth LE Advertising Manager errors.
constexpr char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
constexpr char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
constexpr char kErrorFailed[] = "org.bluez.Error.Failed";
constexpr char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
constexpr char kErrorInvalidLength[] = "org.bluez.Error.InvalidLength";
}  // namespace bluetooth_advertising_manager

namespace bluetooth_debug {
constexpr char kBluetoothDebugInterface[] = "org.chromium.Bluetooth.Debug";

// Methods.
constexpr char kSetLevels[] = "SetLevels";

// Properties.
constexpr char kDispatcherLevelProperty[] = "DispatcherLevel";
constexpr char kNewblueLevelProperty[] = "NewblueLevel";
constexpr char kBluezLevelProperty[] = "BluezLevel";
constexpr char kKernelLevelProperty[] = "KernelLevel";
}  // namespace bluetooth_debug

#endif  // SYSTEM_API_DBUS_BLUETOOTH_DBUS_CONSTANTS_H_
