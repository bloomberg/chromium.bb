// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_

#include <string>

#include "chromeos/dbus/media_transfer_protocol_daemon_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockMediaTransferProtocolDaemonClient
    : public MediaTransferProtocolDaemonClient {
 public:
  MockMediaTransferProtocolDaemonClient();
  virtual ~MockMediaTransferProtocolDaemonClient();

  MOCK_METHOD2(EnumerateStorage, void(const EnumerateStorageCallback&,
                                      const ErrorCallback&));
  MOCK_METHOD3(GetStorageInfo, void(const std::string&,
                                    const GetStorageInfoCallback&,
                                    const ErrorCallback&));
  MOCK_METHOD4(OpenStorage, void(const std::string&,
                                 OpenStorageMode mode,
                                 const OpenStorageCallback&,
                                 const ErrorCallback&));
  MOCK_METHOD3(CloseStorage, void(const std::string&,
                                  const CloseStorageCallback&,
                                  const ErrorCallback&));
  MOCK_METHOD4(ReadDirectoryByPath, void(const std::string&,
                                         const std::string&,
                                         const ReadDirectoryCallback&,
                                         const ErrorCallback&));
  MOCK_METHOD4(ReadDirectoryById, void(const std::string&,
                                       uint32,
                                       const ReadDirectoryCallback&,
                                       const ErrorCallback&));
  MOCK_METHOD4(ReadFileByPath, void(const std::string&,
                                    const std::string&,
                                    const ReadFileCallback&,
                                    const ErrorCallback&));
  MOCK_METHOD4(ReadFileById, void(const std::string&,
                                  uint32,
                                  const ReadFileCallback&,
                                  const ErrorCallback&));
  MOCK_METHOD4(GetFileInfoByPath, void(const std::string&,
                                       const std::string&,
                                       const GetFileInfoCallback&,
                                       const ErrorCallback&));
  MOCK_METHOD4(GetFileInfoById, void(const std::string&,
                                     uint32,
                                     const GetFileInfoCallback&,
                                     const ErrorCallback&));
  MOCK_METHOD1(SetUpConnections, void(const MTPStorageEventHandler&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_
