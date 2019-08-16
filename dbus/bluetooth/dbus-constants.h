// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_BLUETOOTH_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_BLUETOOTH_DBUS_CONSTANTS_H_

namespace bluetooth_plugin {
// Service identifiers for the plugin interface added to the /org/bluez object.
const char kBluetoothPluginServiceName[] = "org.bluez";
const char kBluetoothPluginInterface[] = "org.chromium.Bluetooth";

// Bluetooth plugin properties.
const char kSupportsLEServices[] = "SupportsLEServices";
const char kSupportsConnInfo[] = "SupportsConnInfo";
}  // namespace bluetooth_plugin

namespace bluetooth_plugin_device {
// Service identifiers for the plugin interface added to Bluetooth Device
// objects.
const char kBluetoothPluginServiceName[] = "org.bluez";
const char kBluetoothPluginInterface[] = "org.chromium.BluetoothDevice";

// Bluetooth Device plugin methods.
const char kGetConnInfo[] = "GetConnInfo";
const char kSetLEConnectionParameters[] = "SetLEConnectionParameters";
// Valid connection parameters that can be passed to the
// SetLEConnectionParameters API as dictionary keys.
const char kLEConnectionParameterMinimumConnectionInterval[] =
    "MinimumConnectionInterval";
const char kLEConnectionParameterMaximumConnectionInterval[] =
    "MaximumConnectionInterval";
}  // namespace bluetooth_plugin_device

namespace bluetooth_adapter {
// Bluetooth Adapter service identifiers.
const char kBluetoothAdapterServiceName[] = "org.bluez";
const char kBluetoothAdapterInterface[] = "org.bluez.Adapter1";

// Bluetooth Adapter methods.
const char kStartDiscovery[] = "StartDiscovery";
const char kSetDiscoveryFilter[] = "SetDiscoveryFilter";
const char kStopDiscovery[] = "StopDiscovery";
const char kPauseDiscovery[] = "PauseDiscovery";
const char kUnpauseDiscovery[] = "UnpauseDiscovery";
const char kRemoveDevice[] = "RemoveDevice";
const char kCreateServiceRecord[] = "CreateServiceRecord";
const char kRemoveServiceRecord[] = "RemoveServiceRecord";
const char kHandleSuspendImminent[] = "HandleSuspendImminent";
const char kHandleSuspendDone[] = "HandleSuspendDone";
const char kGetSupportedCapabilities[] = "GetSupportedCapabilities";

// Bluetooth Adapter properties.
const char kAddressProperty[] = "Address";
const char kNameProperty[] = "Name";
const char kAliasProperty[] = "Alias";
const char kClassProperty[] = "Class";
const char kPoweredProperty[] = "Powered";
const char kDiscoverableProperty[] = "Discoverable";
const char kPairableProperty[] = "Pairable";
const char kPairableTimeoutProperty[] = "PairableTimeout";
const char kDiscoverableTimeoutProperty[] = "DiscoverableTimeout";
const char kDiscoveringProperty[] = "Discovering";
const char kUUIDsProperty[] = "UUIDs";
const char kModaliasProperty[] = "Modalias";
const char kStackSyncQuittingProperty[] = "StackSyncQuitting";

// Bluetooth Adapter errors.
const char kErrorNotReady[] = "org.bluez.Error.NotReady";
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";

// Bluetooth Adapter parameters supplied to SetDiscoveryFilter request.
const char kDiscoveryFilterParameterUUIDs[] = "UUIDs";
const char kDiscoveryFilterParameterRSSI[] = "RSSI";
const char kDiscoveryFilterParameterPathloss[] = "Pathloss";
const char kDiscoveryFilterParameterTransport[] = "Transport";
}  // namespace bluetooth_adapter

namespace bluetooth_agent_manager {
// Bluetooth Agent Manager service indentifiers
const char kBluetoothAgentManagerServiceName[] = "org.bluez";
const char kBluetoothAgentManagerServicePath[] = "/org/bluez";
const char kBluetoothAgentManagerInterface[] = "org.bluez.AgentManager1";

// Bluetooth Agent Manager methods.
const char kRegisterAgent[] = "RegisterAgent";
const char kUnregisterAgent[] = "UnregisterAgent";
const char kRequestDefaultAgent[] = "RequestDefaultAgent";

// Bluetooth capabilities.
const char kNoInputNoOutputCapability[] = "NoInputNoOutput";
const char kDisplayOnlyCapability[] = "DisplayOnly";
const char kKeyboardOnlyCapability[] = "KeyboardOnly";
const char kDisplayYesNoCapability[] = "DisplayYesNo";
const char kKeyboardDisplayCapability[] = "KeyboardDisplay";

// Bluetooth Agent Manager errors.
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_agent_manager

namespace bluetooth_agent {
// Bluetooth Agent service indentifiers
const char kBluetoothAgentInterface[] = "org.bluez.Agent1";

// Bluetooth Agent methods.
const char kRelease[] = "Release";
const char kRequestPinCode[] = "RequestPinCode";
const char kDisplayPinCode[] = "DisplayPinCode";
const char kRequestPasskey[] = "RequestPasskey";
const char kDisplayPasskey[] = "DisplayPasskey";
const char kRequestConfirmation[] = "RequestConfirmation";
const char kRequestAuthorization[] = "RequestAuthorization";
const char kAuthorizeService[] = "AuthorizeService";
const char kCancel[] = "Cancel";

// Bluetooth Agent errors.
const char kErrorRejected[] = "org.bluez.Error.Rejected";
const char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_agent

namespace bluetooth_device {
// Bluetooth Device service identifiers.
const char kBluetoothDeviceServiceName[] = "org.bluez";
const char kBluetoothDeviceInterface[] = "org.bluez.Device1";

// Bluetooth Device methods.
const char kConnect[] = "Connect";
const char kDisconnect[] = "Disconnect";
const char kConnectProfile[] = "ConnectProfile";
const char kDisconnectProfile[] = "DisconnectProfile";
const char kPair[] = "Pair";
const char kCancelPairing[] = "CancelPairing";
const char kGetServiceRecords[] = "GetServiceRecords";
const char kExecuteWrite[] = "ExecuteWrite";

// Bluetooth Device properties.
const char kAddressProperty[] = "Address";
const char kNameProperty[] = "Name";
const char kIconProperty[] = "Icon";
const char kClassProperty[] = "Class";
const char kTypeProperty[] = "Type";
const char kAppearanceProperty[] = "Appearance";
const char kUUIDsProperty[] = "UUIDs";
const char kPairedProperty[] = "Paired";
const char kConnectedProperty[] = "Connected";
const char kTrustedProperty[] = "Trusted";
const char kBlockedProperty[] = "Blocked";
const char kAliasProperty[] = "Alias";
const char kAdapterProperty[] = "Adapter";
const char kLegacyPairingProperty[] = "LegacyPairing";
const char kModaliasProperty[] = "Modalias";
const char kRSSIProperty[] = "RSSI";
const char kTxPowerProperty[] = "TxPower";
const char kManufacturerDataProperty[] = "ManufacturerData";
const char kServiceDataProperty[] = "ServiceData";
const char kServicesResolvedProperty[] = "ServicesResolved";
const char kAdvertisingDataFlagsProperty[] = "AdvertisingFlags";
const char kMTUProperty[] = "MTU";
const char kEIRProperty[] = "EIR";

// Bluetooth Device errors.
const char kErrorNotReady[] = "org.bluez.Error.NotReady";
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorAlreadyConnected[] = "org.bluez.Error.AlreadyConnected";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorNotConnected[] = "org.bluez.Error.NotConnected";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";

// Undocumented errors that we know BlueZ returns for Bluetooth Device methods.
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
const char kErrorAuthenticationCanceled[] =
    "org.bluez.Error.AuthenticationCanceled";
const char kErrorAuthenticationFailed[] =
    "org.bluez.Error.AuthenticationFailed";
const char kErrorAuthenticationRejected[] =
    "org.bluez.Error.AuthenticationRejected";
const char kErrorAuthenticationTimeout[] =
    "org.bluez.Error.AuthenticationTimeout";
const char kErrorConnectionAttemptFailed[] =
    "org.bluez.Error.ConnectionAttemptFailed";
}  // namespace bluetooth_device

namespace bluetooth_gatt_characteristic {
// Bluetooth GATT Characteristic service identifiers. The service name is used
// only for characteristic objects hosted by bluetoothd.
const char kBluetoothGattCharacteristicServiceName[] = "org.bluez";
const char kBluetoothGattCharacteristicInterface[] =
    "org.bluez.GattCharacteristic1";

// Bluetooth GATT Characteristic methods.
const char kReadValue[] = "ReadValue";
const char kWriteValue[] = "WriteValue";
const char kStartNotify[] = "StartNotify";
const char kStopNotify[] = "StopNotify";
const char kPrepareWriteValue[] = "PrepareWriteValue";

// Bluetooth GATT Characteristic signals.
const char kValueUpdatedSignal[] = "ValueUpdated";

// Possible keys for option dict used in ReadValue, WriteValue and
// PrepareWriteValue.
const char kOptionOffset[] = "offset";
const char kOptionDevice[] = "device";
const char kOptionHasSubsequentWrite[] = "has-subsequent-write";

// Bluetooth GATT Characteristic properties.
const char kUUIDProperty[] = "UUID";
const char kServiceProperty[] = "Service";
const char kValueProperty[] = "Value";
const char kFlagsProperty[] = "Flags";
const char kNotifyingProperty[] = "Notifying";
const char kDescriptorsProperty[] = "Descriptors";

// Possible values for Bluetooth GATT Characteristic "Flags" property.
const char kFlagBroadcast[] = "broadcast";
const char kFlagRead[] = "read";
const char kFlagWriteWithoutResponse[] = "write-without-response";
const char kFlagWrite[] = "write";
const char kFlagNotify[] = "notify";
const char kFlagIndicate[] = "indicate";
const char kFlagAuthenticatedSignedWrites[] = "authenticated-signed-writes";
const char kFlagExtendedProperties[] = "extended-properties";
const char kFlagReliableWrite[] = "reliable-write";
const char kFlagWritableAuxiliaries[] = "writable-auxiliaries";
const char kFlagEncryptRead[] = "encrypt-read";
const char kFlagEncryptWrite[] = "encrypt-write";
const char kFlagEncryptAuthenticatedRead[] = "encrypt-authenticated-read";
const char kFlagEncryptAuthenticatedWrite[] = "encrypt-authenticated-write";
const char kFlagPermissionRead[] = "permission-read";
const char kFlagPermissionWrite[] = "permission-write";
const char kFlagPermissionEncryptRead[] = "permission-encrypt-read";
const char kFlagPermissionEncryptWrite[] = "permission-encrypt-write";
const char kFlagPermissionAuthenticatedRead[] = "permission-authenticated-read";
const char kFlagPermissionAuthenticatedWrite[] =
    "permission-authenticated-write";
const char kFlagPermissionSecureRead[] = "permission-secure-read";
const char kFlagPermissionSecureWrite[] = "permission-secure-write";

// Bluetooth GATT Characteristic errors.
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorInvalidValueLength[] = "org.bluez.Error.InvalidValueLength";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorNotConnected[] = "org.bluez.Error.NotConnected";
const char kErrorNotPermitted[] = "org.bluez.Error.NotPermitted";
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
}  // namespace bluetooth_gatt_characteristic

namespace bluetooth_gatt_descriptor {
// Bluetooth GATT Descriptor service identifiers. The service name is used
// only for descriptor objects hosted by bluetoothd.
const char kBluetoothGattDescriptorServiceName[] = "org.bluez";
const char kBluetoothGattDescriptorInterface[] = "org.bluez.GattDescriptor1";

// Bluetooth GATT Descriptor methods.
const char kReadValue[] = "ReadValue";
const char kWriteValue[] = "WriteValue";

// Possible keys for option dict used in ReadValue and WriteValue.
const char kOptionOffset[] = "offset";
const char kOptionDevice[] = "device";

// Bluetooth GATT Descriptor properties.
const char kUUIDProperty[] = "UUID";
const char kCharacteristicProperty[] = "Characteristic";
const char kValueProperty[] = "Value";
const char kFlagsProperty[] = "Flags";

// Possible values for Bluetooth GATT Descriptor "Flags" property.
const char kFlagRead[] = "read";
const char kFlagWrite[] = "write";
const char kFlagEncryptRead[] = "encrypt-read";
const char kFlagEncryptWrite[] = "encrypt-write";
const char kFlagEncryptAuthenticatedRead[] = "encrypt-authenticated-read";
const char kFlagEncryptAuthenticatedWrite[] = "encrypt-authenticated-write";

// Bluetooth GATT Descriptor errors.
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorInvalidValueLength[] = "org.bluez.Error.InvalidValueLength";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorNotPermitted[] = "org.bluez.Error.NotPermitted";
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
}  // namespace bluetooth_gatt_descriptor

namespace bluetooth_gatt_manager {
// Bluetooth GATT Manager service identifiers.
const char kBluetoothGattManagerServiceName[] = "org.bluez";
const char kBluetoothGattManagerInterface[] = "org.bluez.GattManager1";

// Bluetooth GATT Manager methods.
const char kRegisterApplication[] = "RegisterApplication";
const char kUnregisterApplication[] = "UnregisterApplication";
const char kRegisterService[] = "RegisterService";
const char kUnregisterService[] = "UnregisterService";

// Bluetooth GATT Manager errors.
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_gatt_manager

namespace bluetooth_gatt_service {
// Bluetooth GATT Service service identifiers. The service name is used
// only for service objects hosted by bluetoothd.
const char kBluetoothGattServiceServiceName[] = "org.bluez";
const char kBluetoothGattServiceInterface[] = "org.bluez.GattService1";

// Bluetooth GATT Service properties.
const char kUUIDProperty[] = "UUID";
const char kDeviceProperty[] = "Device";
const char kPrimaryProperty[] = "Primary";
const char kIncludesProperty[] = "Includes";
const char kCharacteristicsProperty[] = "Characteristics";

// Bluetooth GATT Service errors.
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorInvalidValueLength[] = "org.bluez.Error.InvalidValueLength";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorNotPaired[] = "org.bluez.Error.NotPaired";
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
const char kErrorNotPermitted[] = "org.bluez.Error.NotPermitted";
}  // namespace bluetooth_gatt_service

namespace bluetooth_input {
// Bluetooth Input service identifiers.
const char kBluetoothInputServiceName[] = "org.bluez";
const char kBluetoothInputInterface[] = "org.bluez.Input1";

// Bluetooth Input properties.
const char kReconnectModeProperty[] = "ReconnectMode";

// Bluetooth Input property values.
const char kNoneReconnectModeProperty[] = "none";
const char kHostReconnectModeProperty[] = "host";
const char kDeviceReconnectModeProperty[] = "device";
const char kAnyReconnectModeProperty[] = "any";
}  // namespace bluetooth_input

namespace bluetooth_media {
// Bluetooth Media service identifiers
const char kBluetoothMediaServiceName[] = "org.bluez";
const char kBluetoothMediaInterface[] = "org.bluez.Media1";

// Bluetooth Media methods
const char kRegisterEndpoint[] = "RegisterEndpoint";
const char kUnregisterEndpoint[] = "UnregisterEndpoint";
const char kRegisterPlayer[] = "RegisterPlayer";
const char kUnregisterPlayer[] = "UnregisterPlayer";

// Bluetooth Media errors
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
}  // namespace bluetooth_media

namespace bluetooth_media_endpoint {
// Bluetooth Media Endpoint service identifiers
const char kBluetoothMediaEndpointServiceName[] = "org.bluez";
const char kBluetoothMediaEndpointInterface[] = "org.bluez.MediaEndpoint1";

// Bluetooth Media Endpoint methods
const char kSetConfiguration[] = "SetConfiguration";
const char kSelectConfiguration[] = "SelectConfiguration";
const char kClearConfiguration[] = "ClearConfiguration";
const char kRelease[] = "Release";
}  // namespace bluetooth_media_endpoint

namespace bluetooth_media_transport {
// Bluetooth Media Transport service identifiers
const char kBluetoothMediaTransportServiceName[] = "org.bluez";
const char kBluetoothMediaTransportInterface[] = "org.bluez.MediaTransport1";

// Bluetooth Media Transport methods
const char kAcquire[] = "Acquire";
const char kTryAcquire[] = "TryAcquire";
const char kRelease[] = "Release";

// Bluetooth Media Transport property names.
const char kDeviceProperty[] = "Device";
const char kUUIDProperty[] = "UUID";
const char kCodecProperty[] = "Codec";
const char kConfigurationProperty[] = "Configuration";
const char kStateProperty[] = "State";
const char kDelayProperty[] = "Delay";
const char kVolumeProperty[] = "Volume";

// Possible states for the "State" property
const char kStateIdle[] = "idle";
const char kStatePending[] = "pending";
const char kStateActive[] = "active";

// Bluetooth Media Transport errors.
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorNotAvailable[] = "org.bluez.Error.NotAvailable";
}  // namespace bluetooth_media_transport

namespace bluez_object_manager {
// BlueZ daemon Object Manager service identifiers.
const char kBluezObjectManagerServiceName[] = "org.bluez";
const char kBluezObjectManagerServicePath[] = "/";
}  // namespace bluez_object_manager

namespace bluetooth_object_manager {
// Bluetooth daemon Object Manager service identifiers.
const char kBluetoothObjectManagerServiceName[] = "org.chromium.Bluetooth";
const char kBluetoothObjectManagerServicePath[] = "/";
}  // namespace bluetooth_object_manager

namespace newblue_object_manager {
// NewBlue daemon Object Manager service identifiers.
const char kNewblueObjectManagerServiceName[] = "org.chromium.Newblue";
const char kNewblueObjectManagerServicePath[] = "/";
}  // namespace newblue_object_manager

namespace bluetooth_profile_manager {
// Bluetooth Profile Manager service identifiers.
const char kBluetoothProfileManagerServiceName[] = "org.bluez";
const char kBluetoothProfileManagerServicePath[] = "/org/bluez";
const char kBluetoothProfileManagerInterface[] = "org.bluez.ProfileManager1";

// Bluetooth Profile Manager methods.
const char kRegisterProfile[] = "RegisterProfile";
const char kUnregisterProfile[] = "UnregisterProfile";

// Bluetooth Profile Manager option names.
const char kNameOption[] = "Name";
const char kServiceOption[] = "Service";
const char kRoleOption[] = "Role";
const char kChannelOption[] = "Channel";
const char kPSMOption[] = "PSM";
const char kRequireAuthenticationOption[] = "RequireAuthentication";
const char kRequireAuthorizationOption[] = "RequireAuthorization";
const char kAutoConnectOption[] = "AutoConnect";
const char kServiceRecordOption[] = "ServiceRecord";
const char kVersionOption[] = "Version";
const char kFeaturesOption[] = "Features";

// Bluetooth Profile Manager option values.
const char kClientRoleOption[] = "client";
const char kServerRoleOption[] = "server";

// Bluetooth Profile Manager errors.
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_profile_manager

namespace bluetooth_profile {
// Bluetooth Profile service identifiers.
const char kBluetoothProfileInterface[] = "org.bluez.Profile1";

// Bluetooth Profile methods.
const char kRelease[] = "Release";
const char kNewConnection[] = "NewConnection";
const char kRequestDisconnection[] = "RequestDisconnection";
const char kCancel[] = "Cancel";

// Bluetooth Profile property names.
const char kVersionProperty[] = "Version";
const char kFeaturesProperty[] = "Features";

// Bluetooth Profile errors.
const char kErrorRejected[] = "org.bluez.Error.Rejected";
const char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_profile

namespace bluetooth_advertisement {
// Bluetooth LE Advertisement service identifiers.
const char kBluetoothAdvertisementServiceName[] = "org.bluez";
const char kBluetoothAdvertisementInterface[] = "org.bluez.LEAdvertisement1";

// Bluetooth Advertisement methods.
const char kRelease[] = "Release";

// Bluetooth Advertisement properties.
const char kManufacturerDataProperty[] = "ManufacturerData";
const char kServiceUUIDsProperty[] = "ServiceUUIDs";
const char kServiceDataProperty[] = "ServiceData";
const char kSolicitUUIDsProperty[] = "SolicitUUIDs";
const char kTypeProperty[] = "Type";
const char kIncludeTxPowerProperty[] = "IncludeTxPower";

// Possible values for the "Type" property.
const char kTypeBroadcast[] = "broadcast";
const char kTypePeripheral[] = "peripheral";
}  // namespace bluetooth_advertisement

namespace bluetooth_advertising_manager {
// Bluetooth LE Advertising Manager service identifiers.
const char kBluetoothAdvertisingManagerServiceName[] = "org.bluez";
const char kBluetoothAdvertisingManagerInterface[] =
    "org.bluez.LEAdvertisingManager1";

// Bluetooth LE Advertising Manager methods.
const char kRegisterAdvertisement[] = "RegisterAdvertisement";
const char kUnregisterAdvertisement[] = "UnregisterAdvertisement";
const char kSetAdvertisingIntervals[] = "SetAdvertisingIntervals";
const char kResetAdvertising[] = "ResetAdvertising";

// Bluetooth LE Advertising Manager properties.
const char kIsTXPowerSupportedProperty[] = "IsTXPowerSupported";

// Bluetooth LE Advertising Manager errors.
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorInvalidLength[] = "org.bluez.Error.InvalidLength";
}  // namespace bluetooth_advertising_manager

namespace bluetooth_debug {
const char kBluetoothDebugInterface[] = "org.chromium.Bluetooth.Debug";

// Methods.
const char kSetLevels[] = "SetLevels";

// Properties.
const char kDispatcherLevelProperty[] = "DispatcherLevel";
const char kNewblueLevelProperty[] = "NewblueLevel";
const char kBluezLevelProperty[] = "BluezLevel";
const char kKernelLevelProperty[] = "KernelLevel";
}  // namespace bluetooth_debug

#endif  // SYSTEM_API_DBUS_BLUETOOTH_DBUS_CONSTANTS_H_
