// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/document_scan/document_scan_interface_chromeos.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/fake_lorgnette_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {

namespace api {

// Tests of networking_private_crypto support for Networking Private API.
class DocumentScanInterfaceChromeosTest : public testing::Test {
 public:
  DocumentScanInterfaceChromeosTest() = default;
  ~DocumentScanInterfaceChromeosTest() override = default;

  void SetUp() override {
    scan_interface_.lorgnette_manager_client_ = &client_;
  }

  MOCK_METHOD2(OnListScannersResultReceived,
               void(const std::vector<
                        DocumentScanInterface::ScannerDescription>& scanners,
                    const std::string& error));

  MOCK_METHOD3(OnScanCompleted,
               void(const std::string& scanned_image,
                    const std::string& mime_type,
                    const std::string& error));

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  chromeos::FakeLorgnetteManagerClient client_;
  DocumentScanInterfaceChromeos scan_interface_;
};

MATCHER_P5(IsScannerDescription, name, manufacturer, model, type, mime, "") {
  return arg.name == name && arg.manufacturer == manufacturer &&
         arg.model == model && arg.scanner_type == type &&
         arg.image_mime_type == mime;
}

TEST_F(DocumentScanInterfaceChromeosTest, ListScanners) {
  constexpr char kScannerName[] = "Monet";
  constexpr char kScannerManufacturer[] = "Jacques-Louis David";
  constexpr char kScannerModel[] = "Le Havre";
  constexpr char kScannerType[] = "Impressionism";
  client_.AddScannerTableEntry(
      kScannerName,
      {{lorgnette::kScannerPropertyManufacturer, kScannerManufacturer},
       {lorgnette::kScannerPropertyModel, kScannerModel},
       {lorgnette::kScannerPropertyType, kScannerType}});
  EXPECT_CALL(*this, OnListScannersResultReceived(
                         testing::ElementsAre(IsScannerDescription(
                             kScannerName, kScannerManufacturer, kScannerModel,
                             kScannerType, "image/png")),
                         std::string()));

  scan_interface_.ListScanners(base::Bind(
      &DocumentScanInterfaceChromeosTest::OnListScannersResultReceived,
      base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DocumentScanInterfaceChromeosTest, ScanFailure) {
  constexpr char kScannerName[] = "Monet";
  constexpr int kResolution = 4096;
  EXPECT_CALL(*this, OnScanCompleted("data:image/png;base64,", "image/png",
                                     "Image scan failed"));

  scan_interface_.Scan(
      kScannerName, DocumentScanInterface::kScanModeColor, kResolution,
      base::Bind(&DocumentScanInterfaceChromeosTest::OnScanCompleted,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(DocumentScanInterfaceChromeosTest, ScanSuccess) {
  constexpr char kScannerName[] = "Monet";
  constexpr int kResolution = 4096;
  // Data URL plus base64 representation of "PrettyPicture".
  constexpr char kExpectedImageData[] =
      "data:image/png;base64,UHJldHR5UGljdHVyZQ==";
  client_.AddScanData(kScannerName,
                      chromeos::LorgnetteManagerClient::ScanProperties{
                          lorgnette::kScanPropertyModeColor, kResolution},
                      "PrettyPicture");
  EXPECT_CALL(*this, OnScanCompleted(kExpectedImageData, "image/png", ""));

  scan_interface_.Scan(
      kScannerName, DocumentScanInterface::kScanModeColor, kResolution,
      base::Bind(&DocumentScanInterfaceChromeosTest::OnScanCompleted,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

}  // namespace api

}  // namespace extensions
