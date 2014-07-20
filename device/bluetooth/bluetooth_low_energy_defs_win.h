// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_

#include <windows.h>
#include <cfg.h>
#include <devpkey.h>
#include <setupapi.h>
// #include <bthledef.h>
// TODO(rpaquay):
// bthledef.h from Win8 SDK has a couple of issues when used in a Win32 app:
// * line 420: usage of "pragma pop" instead of "pragma warning(pop)"
// * line 349: no CALLBACK modifier in the definition of
// PFNBLUETOOTH_GATT_EVENT_CALLBACK.
//
// So, we duplicate the definitions we need and prevent the build from including
// the content of bthledef.h.
#ifndef __BTHLEDEF_H__
#define __BTHLEDEF_H__

//
// Bluetooth LE device interface GUID
//
// {781aee18-7733-4ce4-adb0-91f41c67b592}
DEFINE_GUID(GUID_BLUETOOTHLE_DEVICE_INTERFACE,
            0x781aee18,
            0x7733,
            0x4ce4,
            0xad,
            0xd0,
            0x91,
            0xf4,
            0x1c,
            0x67,
            0xb5,
            0x92);

DEFINE_GUID(BTH_LE_ATT_BLUETOOTH_BASE_GUID,
            0x00000000,
            0x0000,
            0x1000,
            0x80,
            0x00,
            0x00,
            0x80,
            0x5F,
            0x9B,
            0x34,
            0xFB);

#define BLUETOOTH_GATT_FLAG_NONE 0x00000000

typedef struct _BTH_LE_UUID {
  BOOLEAN IsShortUuid;
#ifdef MIDL_PASS
  [ switch_type(BOOLEAN), switch_is((BOOLEAN)IsShortUuid) ]
#endif
      union {
#ifdef MIDL_PASS
    [case(TRUE)]
#endif
        USHORT ShortUuid;
#ifdef MIDL_PASS
    [case(FALSE)]
#endif
        GUID LongUuid;
  } Value;
} BTH_LE_UUID, *PBTH_LE_UUID;

typedef struct _BTH_LE_GATT_SERVICE {
  BTH_LE_UUID ServiceUuid;
  USHORT AttributeHandle;
} BTH_LE_GATT_SERVICE, *PBTH_LE_GATT_SERVICE;

typedef struct _BTH_LE_GATT_CHARACTERISTIC {
  USHORT ServiceHandle;
  BTH_LE_UUID CharacteristicUuid;
  USHORT AttributeHandle;
  USHORT CharacteristicValueHandle;
  BOOLEAN IsBroadcastable;
  BOOLEAN IsReadable;
  BOOLEAN IsWritable;
  BOOLEAN IsWritableWithoutResponse;
  BOOLEAN IsSignedWritable;
  BOOLEAN IsNotifiable;
  BOOLEAN IsIndicatable;
  BOOLEAN HasExtendedProperties;
} BTH_LE_GATT_CHARACTERISTIC, *PBTH_LE_GATT_CHARACTERISTIC;

typedef struct _BTH_LE_GATT_CHARACTERISTIC_VALUE {
  ULONG DataSize;

#ifdef MIDL_PASS
  [size_is(DataSize)] UCHAR Data[*];
#else
  _Field_size_bytes_(DataSize) UCHAR Data[1];
#endif
} BTH_LE_GATT_CHARACTERISTIC_VALUE, *PBTH_LE_GATT_CHARACTERISTIC_VALUE;

typedef enum _BTH_LE_GATT_DESCRIPTOR_TYPE {
  CharacteristicExtendedProperties,
  CharacteristicUserDescription,
  ClientCharacteristicConfiguration,
  ServerCharacteristicConfiguration,
  CharacteristicFormat,
  CharacteristicAggregateFormat,
  CustomDescriptor
} BTH_LE_GATT_DESCRIPTOR_TYPE,
    *PBTH_LE_GATT_DESCRIPTOR_TYPE;

typedef struct _BTH_LE_GATT_DESCRIPTOR {
  USHORT ServiceHandle;
  USHORT CharacteristicHandle;
  BTH_LE_GATT_DESCRIPTOR_TYPE DescriptorType;
  BTH_LE_UUID DescriptorUuid;
  USHORT AttributeHandle;
} BTH_LE_GATT_DESCRIPTOR, *PBTH_LE_GATT_DESCRIPTOR;

typedef struct _BTH_LE_GATT_DESCRIPTOR_VALUE {
  BTH_LE_GATT_DESCRIPTOR_TYPE DescriptorType;
  BTH_LE_UUID DescriptorUuid;

#ifdef MIDL_PASS
  [
    switch_type(BTH_LE_GATT_DESCRIPTOR_TYPE),
    switch_is((BTH_LE_GATT_DESCRIPTOR_TYPE)DescriptorType)
  ]
#endif
      union {

#ifdef MIDL_PASS
    [case(CharacteristicExtendedProperties)]
#endif
        struct {
      BOOLEAN IsReliableWriteEnabled;
      BOOLEAN IsAuxiliariesWritable;
    } CharacteristicExtendedProperties;

#ifdef MIDL_PASS
    [case(ClientCharacteristicConfiguration)]
#endif
        struct {
      BOOLEAN IsSubscribeToNotification;
      BOOLEAN IsSubscribeToIndication;
    } ClientCharacteristicConfiguration;

#ifdef MIDL_PASS
    [case(ServerCharacteristicConfiguration)]
#endif
        struct {
      BOOLEAN IsBroadcast;
    } ServerCharacteristicConfiguration;

#ifdef MIDL_PASS
    [case(CharacteristicFormat)]
#endif
        struct {
      UCHAR Format;
      UCHAR Exponent;
      BTH_LE_UUID Unit;
      UCHAR NameSpace;
      BTH_LE_UUID Description;
    } CharacteristicFormat;
#ifdef MIDL_PASS
    [default];
#endif
  };

  ULONG DataSize;

#ifdef MIDL_PASS
  [size_is(DataSize)] UCHAR Data[*];
#else
  _Field_size_bytes_(DataSize) UCHAR Data[1];
#endif
} BTH_LE_GATT_DESCRIPTOR_VALUE, *PBTH_LE_GATT_DESCRIPTOR_VALUE;

typedef enum _BTH_LE_GATT_EVENT_TYPE {
  CharacteristicValueChangedEvent,
} BTH_LE_GATT_EVENT_TYPE;

typedef ULONG64 BTH_LE_GATT_RELIABLE_WRITE_CONTEXT,
    *PBTH_LE_GATT_RELIABLE_WRITE_CONTEXT;

#endif  // __BTHLEDEF_H__
#include <bluetoothapis.h>
#include <bluetoothleapis.h>

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_
