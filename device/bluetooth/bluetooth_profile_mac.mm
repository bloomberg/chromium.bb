// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile_mac.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface IOBluetoothDevice (LionSDKDeclarations)
- (NSString*)addressString;
@end

#endif  // MAC_OS_X_VERSION_10_7

namespace device {
namespace {

const char kNoConnectionCallback[] = "Connection callback not set";
const char kProfileNotFound[] = "Profile not found";

// It's safe to use 0 to represent an unregistered service, as implied by the
// documentation at [ http://goo.gl/YRtCkF ].
const BluetoothSDPServiceRecordHandle kInvalidServiceRecordHandle = 0;

// Converts |uuid| to a IOBluetoothSDPUUID instance.
//
// |uuid| must be in the format of XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.
IOBluetoothSDPUUID* GetIOBluetoothSDPUUID(const std::string& uuid) {
  DCHECK(uuid.size() == 36);
  DCHECK(uuid[8] == '-');
  DCHECK(uuid[13] == '-');
  DCHECK(uuid[18] == '-');
  DCHECK(uuid[23] == '-');
  std::string numbers_only = uuid;
  numbers_only.erase(23, 1);
  numbers_only.erase(18, 1);
  numbers_only.erase(13, 1);
  numbers_only.erase(8, 1);
  std::vector<uint8> uuid_bytes_vector;
  base::HexStringToBytes(numbers_only, &uuid_bytes_vector);
  DCHECK(uuid_bytes_vector.size() == 16);

  return [IOBluetoothSDPUUID uuidWithBytes:&uuid_bytes_vector.front()
                                    length:uuid_bytes_vector.size()];
}

void OnConnectSuccessWithAdapter(
    const base::Closure& callback,
    const BluetoothProfileMac::ConnectionCallback& connection_callback,
    const std::string& device_address,
    scoped_refptr<BluetoothSocket> socket,
    scoped_refptr<BluetoothAdapter> adapter) {
  const BluetoothDevice* device = adapter->GetDevice(device_address);
  if (device) {
    connection_callback.Run(device, socket);
    if (!callback.is_null())
      callback.Run();
  }
}

void OnConnectSuccess(
    const base::Closure& callback,
    const BluetoothProfileMac::ConnectionCallback& connection_callback,
    const std::string& device_address,
    scoped_refptr<BluetoothSocket> socket) {
  BluetoothAdapterFactory::GetAdapter(
      base::Bind(&OnConnectSuccessWithAdapter,
                 callback,
                 connection_callback,
                 device_address,
                 socket));
}

// Converts the given |integer| to a string.
NSString* IntToNSString(int integer) {
  return [[NSNumber numberWithInt:integer] stringValue];
}

// Returns a dictionary containing the Bluetooth service definition
// corresponding to the provided |uuid| and |options|.
// TODO(isherman): Support more of the fields defined in |options|.
NSDictionary* BuildServiceDefinition(const BluetoothUUID& uuid,
                                     const BluetoothProfile::Options& options) {
  NSMutableDictionary* service_definition = [NSMutableDictionary dictionary];

  // TODO(isherman): The service's language is currently hardcoded to English.
  // The language should ideally be specified in the chrome.bluetooth API
  // instead.
  const int kEnglishLanguageBase = 100;
  const int kServiceNameKey =
      kEnglishLanguageBase + kBluetoothSDPAttributeIdentifierServiceName;
  NSString* service_name = base::SysUTF8ToNSString(options.name);
  [service_definition setObject:service_name
                         forKey:IntToNSString(kServiceNameKey)];

  const int kUUIDsKey = kBluetoothSDPAttributeIdentifierServiceClassIDList;
  NSArray* uuids = @[GetIOBluetoothSDPUUID(uuid.canonical_value())];
  [service_definition setObject:uuids forKey:IntToNSString(kUUIDsKey)];

  const int kProtocolDefinitionsKey =
      kBluetoothSDPAttributeIdentifierProtocolDescriptorList;
  NSArray* rfcomm_protocol_definition =
      @[[IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16RFCOMM],
        [NSNumber numberWithInt:options.channel]];
  [service_definition setObject:@[rfcomm_protocol_definition]
                         forKey:IntToNSString(kProtocolDefinitionsKey)];

  return service_definition;
}

// Registers a Bluetooth service with the specifieid |uuid| and |options| in the
// system SDP server. Returns a handle to the registered service, or
// |kInvalidServcieRecordHandle| if the service could not be registered.
BluetoothSDPServiceRecordHandle RegisterService(
    const BluetoothUUID& uuid,
    const BluetoothProfile::Options& options) {
  // Attempt to register the service.
  NSDictionary* service_definition = BuildServiceDefinition(uuid, options);
  IOBluetoothSDPServiceRecordRef service_record_ref;
  IOReturn result =
      IOBluetoothAddServiceDict((CFDictionaryRef)service_definition,
                                &service_record_ref);
  if (result != kIOReturnSuccess)
    return kInvalidServiceRecordHandle;
  // Transfer ownership to a scoped object, to simplify memory management.
  base::ScopedCFTypeRef<IOBluetoothSDPServiceRecordRef>
      scoped_service_record_ref(service_record_ref);

  // Extract the service record handle.
  BluetoothSDPServiceRecordHandle service_record_handle;
  IOBluetoothSDPServiceRecord* service_record =
      [IOBluetoothSDPServiceRecord withSDPServiceRecordRef:service_record_ref];
  result = [service_record getServiceRecordHandle:&service_record_handle];
  if (result != kIOReturnSuccess)
    return kInvalidServiceRecordHandle;

  // Verify that the requested channel id was available. If not, withdraw the
  // service.
  // TODO(isherman): Once the chrome.bluetooth API is updated, it should be
  // possible to register an RFCOMM service without explicitly specifying a
  // channel. In that case, we should be willing to bind to any channel, and not
  // try to require any specific channel as we currently do here.
  BluetoothRFCOMMChannelID rfcomm_channel_id;
  result = [service_record getRFCOMMChannelID:&rfcomm_channel_id];
  if (result != kIOReturnSuccess || rfcomm_channel_id != options.channel) {
    IOBluetoothRemoveServiceWithRecordHandle(service_record_handle);
    return kInvalidServiceRecordHandle;
  }

  return service_record_handle;
}

}  // namespace
}  // namespace device

// A simple helper class that forwards system notifications to its wrapped
// |profile_|.
@interface BluetoothProfileMacHelper : NSObject {
 @private
  // The profile that owns |self|.
  device::BluetoothProfileMac* profile_;  // weak

  // The OS mechanism used to subscribe to and unsubscribe from RFCOMM channel
  // creation notifications.
  IOBluetoothUserNotification* rfcommNewChannelNotification_;  // weak
}

- (id)initWithProfile:(device::BluetoothProfileMac*)profile
            channelID:(BluetoothRFCOMMChannelID)channelID;
- (void)rfcommChannelOpened:(IOBluetoothUserNotification*)notification
                    channel:(IOBluetoothRFCOMMChannel*)rfcommChannel;

@end

@implementation BluetoothProfileMacHelper

- (id)initWithProfile:(device::BluetoothProfileMac*)profile
            channelID:(BluetoothRFCOMMChannelID)channelID {
  if ((self = [super init])) {
    profile_ = profile;

    SEL selector = @selector(rfcommChannelOpened:channel:);
    const auto kIncomingDirection =
        kIOBluetoothUserNotificationChannelDirectionIncoming;
    rfcommNewChannelNotification_ =
        [IOBluetoothRFCOMMChannel
          registerForChannelOpenNotifications:self
                                     selector:selector
                                withChannelID:channelID
                                    direction:kIncomingDirection];
  }

  return self;
}

- (void)dealloc {
  [rfcommNewChannelNotification_ unregister];
  [super dealloc];
}

- (void)rfcommChannelOpened:(IOBluetoothUserNotification*)notification
                    channel:(IOBluetoothRFCOMMChannel*)rfcommChannel {
  if (notification != rfcommNewChannelNotification_) {
    // This case is reachable if there are pre-existing RFCOMM channels open at
    // the time that the helper is created. In that case, each existing channel
    // calls into this method with a different notification than the one this
    // class registered with. Ignore those; this class is only interested in
    // channels that have opened since it registered for notifications.
    return;
  }

  profile_->OnRFCOMMChannelOpened(rfcommChannel);
}

@end

namespace device {

BluetoothProfile* CreateBluetoothProfileMac(
    const BluetoothUUID& uuid,
    const BluetoothProfile::Options& options) {
  return new BluetoothProfileMac(uuid, options);
}

BluetoothProfileMac::BluetoothProfileMac(const BluetoothUUID& uuid,
                                         const Options& options)
    : uuid_(uuid),
      rfcomm_channel_id_(options.channel),
      helper_([[BluetoothProfileMacHelper alloc]
                initWithProfile:this
                      channelID:rfcomm_channel_id_]),
      service_record_handle_(RegisterService(uuid, options)) {
  // TODO(isherman): What should happen if there was an error registering the
  // service, i.e. if |service_record_handle_| is |kInvalidServiceRecordHandle|?
  // http://crbug.com/367290
}

BluetoothProfileMac::~BluetoothProfileMac() {
  if (service_record_handle_ != kInvalidServiceRecordHandle)
    IOBluetoothRemoveServiceWithRecordHandle(service_record_handle_);
}

void BluetoothProfileMac::Unregister() {
  delete this;
}

void BluetoothProfileMac::SetConnectionCallback(
    const ConnectionCallback& callback) {
  connection_callback_ = callback;
}

void BluetoothProfileMac::Connect(
    IOBluetoothDevice* device,
    const base::Closure& success_callback,
    const BluetoothSocket::ErrorCompletionCallback& error_callback) {
  if (connection_callback_.is_null()) {
    error_callback.Run(kNoConnectionCallback);
    return;
  }

  IOBluetoothSDPServiceRecord* record = [device
      getServiceRecordForUUID:GetIOBluetoothSDPUUID(uuid_.canonical_value())];
  if (record == nil) {
    error_callback.Run(kProfileNotFound);
    return;
  }

  std::string device_address = base::SysNSStringToUTF8([device addressString]);
  BluetoothSocketMac::Connect(
      record,
      base::Bind(OnConnectSuccess,
                 success_callback,
                 connection_callback_,
                 device_address),
      error_callback);
}

void BluetoothProfileMac::OnRFCOMMChannelOpened(
    IOBluetoothRFCOMMChannel* rfcomm_channel) {
  DCHECK_EQ([rfcomm_channel getChannelID], rfcomm_channel_id_);
  std::string device_address =
      base::SysNSStringToUTF8([[rfcomm_channel getDevice] addressString]);
  BluetoothSocketMac::AcceptConnection(
      rfcomm_channel,
      base::Bind(OnConnectSuccess,
                 base::Closure(),
                 connection_callback_,
                 device_address),
      BluetoothSocket::ErrorCompletionCallback());

  // TODO(isherman): Currently, both the profile and the socket remain alive
  // even after the app that requested them is closed. That's not great, as a
  // misbehaving app could saturate all of the system's RFCOMM channels, and
  // then they would not be freed until the user restarts Chrome.
  // http://crbug.com/367316
  // TODO(isherman): Likewise, the socket currently remains alive even if the
  // underlying rfcomm_channel is closed, e.g. via the client disconnecting, or
  // the user closing the Bluetooth connection via the system menu. This
  // functions essentially as a minor memory leak.
  // http://crbug.com/367319
}

}  // namespace device
