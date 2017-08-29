// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac_metrics.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include "base/metrics/histogram_macros.h"

namespace {

#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
const NSInteger CBErrorMaxConnection = 10;
#endif  // MAC_OS_X_VERSION_10_11

MacOSBluetoothOperationsResult GetMacOSOperationResultFromNSError(
    NSError* error) {
  if (!error)
    return MacOSBluetoothOperationsResult::NO_ERROR;
  NSString* error_domain = [error domain];
  NSInteger error_code = [error code];
  if ([error_domain isEqualToString:CBErrorDomain]) {
    CBError cb_error_code = static_cast<CBError>(error_code);
    switch (cb_error_code) {
      case CBErrorUnknown:
        return MacOSBluetoothOperationsResult::CBERROR_UNKNOWN;
      case CBErrorInvalidParameters:
        return MacOSBluetoothOperationsResult::CBERROR_INVALID_PARAMETERS;
      case CBErrorInvalidHandle:
        return MacOSBluetoothOperationsResult::CBERROR_INVALID_HANDLE;
      case CBErrorNotConnected:
        return MacOSBluetoothOperationsResult::CBERROR_NOT_CONNECTED;
      case CBErrorOutOfSpace:
        return MacOSBluetoothOperationsResult::CBERROR_OUT_OF_SPACE;
      case CBErrorOperationCancelled:
        return MacOSBluetoothOperationsResult::CBERROR_OPERATION_CANCELLED;
      case CBErrorConnectionTimeout:
        return MacOSBluetoothOperationsResult::CBERROR_CONNECTION_TIMEOUT;
      case CBErrorPeripheralDisconnected:
        return MacOSBluetoothOperationsResult::CBERROR_PERIPHERAL_DISCONNECTED;
      case CBErrorUUIDNotAllowed:
        return MacOSBluetoothOperationsResult::CBERROR_UUID_NOT_ALLOWED;
      case CBErrorAlreadyAdvertising:
        return MacOSBluetoothOperationsResult::CBERROR_ALREADY_ADVERTISING;
      default:
        if (@available(macOS 10.11, *)) {
          if (CBErrorMaxConnection == cb_error_code)
            return MacOSBluetoothOperationsResult::CBERROR_MAX_CONNECTION;
        }
        NOTREACHED();
    }
    return MacOSBluetoothOperationsResult::CBATT_ERROR_UNKNOWN_ERROR_CODE;
  } else if ([error_domain isEqualToString:CBATTErrorDomain]) {
    switch (static_cast<CBATTError>(error_code)) {
      case CBATTErrorSuccess:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_SUCCESS;
      case CBATTErrorInvalidHandle:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_INVALID_HANDLE;
      case CBATTErrorReadNotPermitted:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_READ_NOT_PERMITTED;
      case CBATTErrorWriteNotPermitted:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_WRITE_NOT_PERMITTED;
      case CBATTErrorInvalidPdu:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_INVALID_PDU;
      case CBATTErrorInsufficientAuthentication:
        return MacOSBluetoothOperationsResult::
            CBATT_ERROR_INSUFFICIENT_AUTHENTICATION;
      case CBATTErrorRequestNotSupported:
        return MacOSBluetoothOperationsResult::
            CBATT_ERROR_REQUEST_NOT_SUPPORTED;
      case CBATTErrorInvalidOffset:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_INVALID_OFFSET;
      case CBATTErrorInsufficientAuthorization:
        return MacOSBluetoothOperationsResult::
            CBATT_ERROR_INSUFFICIENT_AUTHORIZATION;
      case CBATTErrorPrepareQueueFull:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_PREPARE_QUEUE_FULL;
      case CBATTErrorAttributeNotFound:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_ATTRIBUTE_NOT_FOUND;
      case CBATTErrorAttributeNotLong:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_ATTRIBUTE_NOT_LONG;
      case CBATTErrorInsufficientEncryptionKeySize:
        return MacOSBluetoothOperationsResult::
            CBATT_ERROR_INSUFFICIENT_ENCRYPTION_KEY_SIZE;
      case CBATTErrorInvalidAttributeValueLength:
        return MacOSBluetoothOperationsResult::
            CBATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
      case CBATTErrorUnlikelyError:
        return MacOSBluetoothOperationsResult::CBATT_ERROR_UNLIKELY_ERROR;
      case CBATTErrorInsufficientEncryption:
        return MacOSBluetoothOperationsResult::
            CBATT_ERROR_INSUFFICIENT_ENCRYPTION;
      case CBATTErrorUnsupportedGroupType:
        return MacOSBluetoothOperationsResult::
            CBATT_ERROR_UNSUPPORTED_GROUP_TYPE;
      case CBATTErrorInsufficientResources:
        return MacOSBluetoothOperationsResult::
            CBATT_ERROR_INSUFFICIENT_RESOURCES;
    }
    return MacOSBluetoothOperationsResult::CBERROR_UNKNOWN_ERROR_CODE;
  }
  // TODO(crbug.com/755667): Needs to create an histogram to record unknown
  // error domains.
  return MacOSBluetoothOperationsResult::UNKNOWN_ERROR_DOMAIN;
}

}  // namespace

void RecordDidFailToConnectPeripheralResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidFailToConnectToPeripheral",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidDisconnectPeripheralResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidDisconnectPeripheral",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidDiscoverPrimaryServicesResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidDiscoverPrimaryServices",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidDiscoverCharacteristicsResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidDiscoverCharacteristics",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidUpdateValueResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidUpdateValue",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidWriteValueResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidWriteValue",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidUpdateNotificationStateResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidUpdateNotificationState",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidDiscoverDescriptorsResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidDiscoverDescriptors",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidUpdateValueForDescriptorResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidUpdateValueForDescriptor",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}

void RecordDidWriteValueForDescriptorResult(NSError* error) {
  MacOSBluetoothOperationsResult histogram_macos_error =
      GetMacOSOperationResultFromNSError(error);
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.MacOS.Errors.DidWriteValueForDescriptor",
      static_cast<int>(histogram_macos_error),
      static_cast<int>(MacOSBluetoothOperationsResult::MAX));
}
