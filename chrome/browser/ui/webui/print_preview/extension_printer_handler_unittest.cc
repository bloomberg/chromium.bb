// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/test/test_utils.h"
#include "device/base/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "extensions/common/extension.h"
#include "extensions/common/value_builder.h"
#include "printing/pdf_render_settings.h"
#include "printing/print_job_constants.h"
#include "printing/pwg_raster_settings.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using device::MockUsbDevice;
using device::MockUsbService;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::PrinterProviderAPI;
using extensions::PrinterProviderPrintJob;
using extensions::TestExtensionEnvironment;
using printing::PwgRasterConverter;

namespace {

// Printer id used for requests in tests.
const char kPrinterId[] = "printer_id";

// Printer list used a result for getPrinters.
const char kPrinterDescriptionList[] =
    "[{"
    "  \"id\": \"printer1\","
    "  \"name\": \"Printer 1\""
    "}, {"
    "  \"id\": \"printer2\","
    "  \"name\": \"Printer 2\","
    "  \"description\": \"Test printer 2\""
    "}]";

// Printer capability for printer that supports all content types.
const char kAllContentTypesSupportedPrinter[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"printer\": {"
    "    \"supported_content_type\": ["
    "      {\"content_type\": \"*/*\"}"
    "    ]"
    "  }"
    "}";

// Printer capability for a printer that supports PDF.
const char kPdfSupportedPrinter[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"printer\": {"
    "    \"supported_content_type\": ["
    "      {\"content_type\": \"application/pdf\"},"
    "      {\"content_type\": \"image/pwg-raster\"}"
    "    ]"
    "  }"
    "}";

// Printer capability for a printer that supportd only PWG raster.
const char kPWGRasterOnlyPrinterSimpleDescription[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"printer\": {"
    "    \"supported_content_type\": ["
    "      {\"content_type\": \"image/pwg-raster\"}"
    "    ]"
    "  }"
    "}";

// Printer capability for a printer that supportd only PWG raster that has
// options other that supported_content_type set.
const char kPWGRasterOnlyPrinter[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"printer\": {"
    "    \"supported_content_type\": ["
    "      {\"content_type\": \"image/pwg-raster\"}"
    "    ],"
    "    \"pwg_raster_config\": {"
    "      \"document_sheet_back\": \"FLIPPED\","
    "      \"reverse_order_streaming\": true,"
    "      \"rotate_all_pages\": true"
    "    },"
    "    \"dpi\": {"
    "      \"option\": [{"
    "        \"horizontal_dpi\": 100,"
    "        \"vertical_dpi\": 200,"
    "        \"is_default\": true"
    "      }]"
    "    }"
    "  }"
    "}";

// Print ticket with no parameters set.
const char kEmptyPrintTicket[] = "{\"version\": \"1.0\"}";

// Print ticket that has duplex parameter set.
const char kPrintTicketWithDuplex[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"print\": {"
    "    \"duplex\": {\"type\": \"LONG_EDGE\"}"
    "  }"
    "}";

// An extension with permission for 1 printer it supports.
const char kExtension1[] =
    "{"
    "  \"name\": \"Provider 1\","
    "  \"app\": {"
    "    \"background\": {"
    "      \"scripts\": [\"background.js\"]"
    "    }"
    "  },"
    "  \"permissions\": ["
    "    \"printerProvider\","
    "    \"usb\","
    "    {"
    "     \"usbDevices\": ["
    "       { \"vendorId\": 0, \"productId\": 1 }"
    "     ]"
    "    },"
    "  ],"
    "  \"usb_printers\": {"
    "    \"filters\": ["
    "      { \"vendorId\": 0, \"productId\": 0 },"
    "      { \"vendorId\": 0, \"productId\": 1 }"
    "    ]"
    "  }"
    "}";

// An extension with permission for none of the printers it supports.
const char kExtension2[] =
    "{"
    "  \"name\": \"Provider 2\","
    "  \"app\": {"
    "    \"background\": {"
    "      \"scripts\": [\"background.js\"]"
    "    }"
    "  },"
    "  \"permissions\": [ \"printerProvider\", \"usb\" ],"
    "  \"usb_printers\": {"
    "    \"filters\": ["
    "      { \"vendorId\": 0, \"productId\": 0 },"
    "      { \"vendorId\": 0, \"productId\": 1 }"
    "    ]"
    "  }"
    "}";

const char kContentTypePDF[] = "application/pdf";
const char kContentTypePWG[] = "image/pwg-raster";

// Print request status considered to be successful by fake PrinterProviderAPI.
const char kPrintRequestSuccess[] = "OK";

constexpr unsigned char kPrintData[] = "print data, PDF";
constexpr size_t kPrintDataLength = sizeof(kPrintData);

// Used as a callback to StartGetPrinters in tests.
// Increases |*call_count| and records values returned by StartGetPrinters.
void RecordPrinterList(size_t* call_count,
                       std::unique_ptr<base::ListValue>* printers_out,
                       const base::ListValue& printers) {
  ++(*call_count);
  printers_out->reset(printers.DeepCopy());
}

// Used as a callback to StartGetPrinters in tests.
// Records that the test is done.
void RecordPrintersDone(bool* is_done_out) {
  *is_done_out = true;
}

// Used as a callback to StartGetCapability in tests.
// Increases |*call_count| and records values returned by StartGetCapability.
void RecordCapability(size_t* call_count,
                      std::unique_ptr<base::DictionaryValue>* capability_out,
                      std::unique_ptr<base::DictionaryValue> capability) {
  ++(*call_count);
  const base::Value* capabilities = nullptr;
  if (capability) {
    capabilities = capability->FindKeyOfType(printing::kSettingCapabilities,
                                             base::Value::Type::DICTIONARY);
  }
  *capability_out =
      capabilities ? base::DictionaryValue::From(
                         std::make_unique<base::Value>(capabilities->Clone()))
                   : nullptr;
}

// Used as a callback to StartPrint in tests.
// Increases |*call_count| and records values returned by StartPrint.
void RecordPrintResult(size_t* call_count,
                       bool* success_out,
                       std::string* status_out,
                       const base::Value& status) {
  ++(*call_count);
  bool success = status.is_none();
  std::string status_str = success ? kPrintRequestSuccess : status.GetString();
  *success_out = success;
  *status_out = status_str;
}

// Used as a callback to StartGrantPrinterAccess in tests.
// Increases |*call_count| and records the value returned.
void RecordPrinterInfo(size_t* call_count,
                       std::unique_ptr<base::DictionaryValue>* printer_info_out,
                       const base::DictionaryValue& printer_info) {
  ++(*call_count);
  printer_info_out->reset(printer_info.DeepCopy());
}

// Converts JSON string to base::ListValue object.
// On failure, returns NULL and fills |*error| string.
std::unique_ptr<base::ListValue> GetJSONAsListValue(const std::string& json,
                                                    std::string* error) {
  auto ret = base::ListValue::From(
      JSONStringValueDeserializer(json).Deserialize(nullptr, error));
  if (!ret)
    *error = "Value is not a list.";
  return ret;
}

// Converts JSON string to base::DictionaryValue object.
// On failure, returns NULL and fills |*error| string.
std::unique_ptr<base::DictionaryValue> GetJSONAsDictionaryValue(
    const std::string& json,
    std::string* error) {
  auto ret = base::DictionaryValue::From(
      JSONStringValueDeserializer(json).Deserialize(nullptr, error));
  if (!ret)
    *error = "Value is not a dictionary.";
  return ret;
}

std::string RefCountedMemoryToString(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  return std::string(memory->front_as<char>(), memory->size());
}

// Fake PwgRasterConverter used in the tests.
class FakePwgRasterConverter : public PwgRasterConverter {
 public:
  FakePwgRasterConverter() : fail_conversion_(false), initialized_(false) {}
  ~FakePwgRasterConverter() override = default;

  // PwgRasterConverter implementation. It writes |data| to a temp file.
  // Also, remembers conversion and bitmap settings passed into the method.
  void Start(base::RefCountedMemory* data,
             const printing::PdfRenderSettings& conversion_settings,
             const printing::PwgRasterSettings& bitmap_settings,
             ResultCallback callback) override {
    if (fail_conversion_) {
      std::move(callback).Run(false, base::FilePath());
      return;
    }

    if (!initialized_ && !temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "Unable to create target dir for cenverter";
      std::move(callback).Run(false, base::FilePath());
      return;
    }

    initialized_ = true;

    path_ = temp_dir_.GetPath().AppendASCII("output.pwg");
    std::string data_str(data->front_as<char>(), data->size());
    int written = WriteFile(path_, data_str.c_str(), data_str.size());
    if (written != static_cast<int>(data_str.size())) {
      ADD_FAILURE() << "Failed to write pwg raster file.";
      std::move(callback).Run(false, base::FilePath());
      return;
    }

    conversion_settings_ = conversion_settings;
    bitmap_settings_ = bitmap_settings;

    std::move(callback).Run(true, path_);
  }

  // Makes |Start| method always return an error.
  void FailConversion() { fail_conversion_ = true; }

  const base::FilePath& path() { return path_; }
  const printing::PdfRenderSettings& conversion_settings() const {
    return conversion_settings_;
  }

  const printing::PwgRasterSettings& bitmap_settings() const {
    return bitmap_settings_;
  }

 private:
  base::ScopedTempDir temp_dir_;

  base::FilePath path_;
  printing::PdfRenderSettings conversion_settings_;
  printing::PwgRasterSettings bitmap_settings_;
  bool fail_conversion_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(FakePwgRasterConverter);
};

// Information about received print requests.
struct PrintRequestInfo {
  PrinterProviderAPI::PrintCallback callback;
  PrinterProviderPrintJob job;
};

// Fake PrinterProviderAPI used in tests.
// It caches requests issued to API and exposes methods to trigger their
// callbacks.
class FakePrinterProviderAPI : public PrinterProviderAPI {
 public:
  FakePrinterProviderAPI() = default;
  ~FakePrinterProviderAPI() override = default;

  void DispatchGetPrintersRequested(
      const PrinterProviderAPI::GetPrintersCallback& callback) override {
    pending_printers_callbacks_.push(callback);
  }

  void DispatchGetCapabilityRequested(
      const std::string& destination_id,
      PrinterProviderAPI::GetCapabilityCallback callback) override {
    pending_capability_callbacks_.push(std::move(callback));
  }

  void DispatchPrintRequested(
      const PrinterProviderPrintJob& job,
      PrinterProviderAPI::PrintCallback callback) override {
    PrintRequestInfo request_info;
    request_info.callback = std::move(callback);
    request_info.job = job;

    pending_print_requests_.push(std::move(request_info));
  }

  void DispatchGetUsbPrinterInfoRequested(
      const std::string& extension_id,
      scoped_refptr<device::UsbDevice> device,
      PrinterProviderAPI::GetPrinterInfoCallback callback) override {
    EXPECT_EQ("fake extension id", extension_id);
    EXPECT_TRUE(device);
    pending_usb_info_callbacks_.push(std::move(callback));
  }

  size_t pending_get_printers_count() const {
    return pending_printers_callbacks_.size();
  }

  const PrinterProviderPrintJob* GetPrintJob(
      const extensions::Extension* extension,
      int request_id) const override {
    ADD_FAILURE() << "Not reached";
    return nullptr;
  }

  void TriggerNextGetPrintersCallback(const base::ListValue& printers,
                                      bool done) {
    ASSERT_GT(pending_get_printers_count(), 0u);
    pending_printers_callbacks_.front().Run(printers, done);
    pending_printers_callbacks_.pop();
  }

  size_t pending_get_capability_count() const {
    return pending_capability_callbacks_.size();
  }

  void TriggerNextGetCapabilityCallback(
      const base::DictionaryValue& description) {
    ASSERT_GT(pending_get_capability_count(), 0u);
    std::move(pending_capability_callbacks_.front()).Run(description);
    pending_capability_callbacks_.pop();
  }

  size_t pending_print_count() const { return pending_print_requests_.size(); }

  const PrinterProviderPrintJob* GetNextPendingPrintJob() const {
    EXPECT_GT(pending_print_count(), 0u);
    if (pending_print_count() == 0)
      return NULL;
    return &pending_print_requests_.front().job;
  }

  void TriggerNextPrintCallback(const std::string& result) {
    ASSERT_GT(pending_print_count(), 0u);
    base::Value result_value;
    if (result != kPrintRequestSuccess)
      result_value = base::Value(result);
    std::move(pending_print_requests_.front().callback).Run(result_value);
    pending_print_requests_.pop();
  }

  size_t pending_usb_info_count() const {
    return pending_usb_info_callbacks_.size();
  }

  void TriggerNextUsbPrinterInfoCallback(
      const base::DictionaryValue& printer_info) {
    ASSERT_GT(pending_usb_info_count(), 0u);
    std::move(pending_usb_info_callbacks_.front()).Run(printer_info);
    pending_usb_info_callbacks_.pop();
  }

 private:
  base::queue<PrinterProviderAPI::GetPrintersCallback>
      pending_printers_callbacks_;
  base::queue<PrinterProviderAPI::GetCapabilityCallback>
      pending_capability_callbacks_;
  base::queue<PrintRequestInfo> pending_print_requests_;
  base::queue<PrinterProviderAPI::GetPrinterInfoCallback>
      pending_usb_info_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FakePrinterProviderAPI);
};

std::unique_ptr<KeyedService> BuildTestingPrinterProviderAPI(
    content::BrowserContext* context) {
  return std::make_unique<FakePrinterProviderAPI>();
}

}  // namespace

class ExtensionPrinterHandlerTest : public testing::Test {
 public:
  ExtensionPrinterHandlerTest() = default;
  ~ExtensionPrinterHandlerTest() override = default;

  void SetUp() override {
    extensions::PrinterProviderAPIFactory::GetInstance()->SetTestingFactory(
        env_.profile(), &BuildTestingPrinterProviderAPI);
    extension_printer_handler_ =
        std::make_unique<ExtensionPrinterHandler>(env_.profile());

    auto pwg_raster_converter = std::make_unique<FakePwgRasterConverter>();
    pwg_raster_converter_ = pwg_raster_converter.get();
    extension_printer_handler_->SetPwgRasterConverterForTesting(
        std::move(pwg_raster_converter));
  }

 protected:
  FakePrinterProviderAPI* GetPrinterProviderAPI() {
    return static_cast<FakePrinterProviderAPI*>(
        extensions::PrinterProviderAPIFactory::GetInstance()
            ->GetForBrowserContext(env_.profile()));
  }

  device::MockUsbService& usb_service() {
    return *device_client_.usb_service();
  }

  device::MockDeviceClient device_client_;
  TestExtensionEnvironment env_;
  std::unique_ptr<ExtensionPrinterHandler> extension_printer_handler_;

  // Owned by |extension_printer_handler_|.
  FakePwgRasterConverter* pwg_raster_converter_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionPrinterHandlerTest);
};

TEST_F(ExtensionPrinterHandlerTest, GetPrinters) {
  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;

  extension_printer_handler_->StartGetPrinters(
      base::Bind(&RecordPrinterList, &call_count, &printers),
      base::Bind(&RecordPrintersDone, &is_done));

  EXPECT_FALSE(printers.get());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_printers_count());

  std::string error;
  std::unique_ptr<base::ListValue> original_printers(
      GetJSONAsListValue(kPrinterDescriptionList, &error));
  ASSERT_TRUE(original_printers) << "Failed to deserialize printers: " << error;

  fake_api->TriggerNextGetPrintersCallback(*original_printers, true);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(is_done);
  ASSERT_TRUE(printers.get());
  EXPECT_TRUE(printers->Equals(original_printers.get()))
      << *printers << ", expected: " << *original_printers;
}

TEST_F(ExtensionPrinterHandlerTest, GetPrinters_Reset) {
  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;

  extension_printer_handler_->StartGetPrinters(
      base::Bind(&RecordPrinterList, &call_count, &printers),
      base::Bind(&RecordPrintersDone, &is_done));

  EXPECT_FALSE(printers.get());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_printers_count());

  extension_printer_handler_->Reset();

  std::string error;
  std::unique_ptr<base::ListValue> original_printers(
      GetJSONAsListValue(kPrinterDescriptionList, &error));
  ASSERT_TRUE(original_printers) << "Error deserializing printers: " << error;

  fake_api->TriggerNextGetPrintersCallback(*original_printers, true);

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, GetUsbPrinters) {
  auto device0 =
      base::MakeRefCounted<MockUsbDevice>(0, 0, "Google", "USB Printer", "");
  usb_service().AddDevice(device0);
  auto device1 =
      base::MakeRefCounted<MockUsbDevice>(0, 1, "Google", "USB Printer", "");
  usb_service().AddDevice(device1);

  const Extension* extension_1 = env_.MakeExtension(
      *base::test::ParseJson(kExtension1), "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const Extension* extension_2 = env_.MakeExtension(
      *base::test::ParseJson(kExtension2), "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

  extensions::DevicePermissionsManager* permissions_manager =
      extensions::DevicePermissionsManager::Get(env_.profile());
  permissions_manager->AllowUsbDevice(extension_2->id(), device0);

  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;
  extension_printer_handler_->StartGetPrinters(
      base::Bind(&RecordPrinterList, &call_count, &printers),
      base::Bind(&RecordPrintersDone, &is_done));

  base::RunLoop().RunUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_printers_count());

  EXPECT_EQ(1u, call_count);
  EXPECT_FALSE(is_done);
  EXPECT_TRUE(printers.get());
  EXPECT_EQ(2u, printers->GetSize());
  std::unique_ptr<base::DictionaryValue> extension_1_entry(
      DictionaryBuilder()
          .Set("id", base::StringPrintf("provisional-usb:%s:%s",
                                        extension_1->id().c_str(),
                                        device0->guid().c_str()))
          .Set("name", "USB Printer")
          .Set("extensionName", "Provider 1")
          .Set("extensionId", extension_1->id())
          .SetBoolean("provisional", true)
          .Build());
  std::unique_ptr<base::DictionaryValue> extension_2_entry(
      DictionaryBuilder()
          .Set("id", base::StringPrintf("provisional-usb:%s:%s",
                                        extension_2->id().c_str(),
                                        device1->guid().c_str()))
          .Set("name", "USB Printer")
          .Set("extensionName", "Provider 2")
          .Set("extensionId", extension_2->id())
          .SetBoolean("provisional", true)
          .Build());
  EXPECT_TRUE(printers->Find(*extension_1_entry) != printers->end());
  EXPECT_TRUE(printers->Find(*extension_2_entry) != printers->end());

  fake_api->TriggerNextGetPrintersCallback(base::ListValue(), true);

  EXPECT_EQ(1u, call_count);  // No printers, so no calls. Call count stays 1.
  EXPECT_TRUE(is_done);       // Still calls done.
  EXPECT_TRUE(printers.get());
}

TEST_F(ExtensionPrinterHandlerTest, GetCapability) {
  size_t call_count = 0;
  std::unique_ptr<base::DictionaryValue> capability;

  extension_printer_handler_->StartGetCapability(
      kPrinterId, base::Bind(&RecordCapability, &call_count, &capability));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_capability_count());

  std::string error;
  std::unique_ptr<base::DictionaryValue> original_capability(
      GetJSONAsDictionaryValue(kPWGRasterOnlyPrinterSimpleDescription, &error));
  ASSERT_TRUE(original_capability)
      << "Error deserializing capability: " << error;

  fake_api->TriggerNextGetCapabilityCallback(*original_capability);

  EXPECT_EQ(1u, call_count);
  ASSERT_TRUE(capability.get());
  EXPECT_TRUE(capability->Equals(original_capability.get()))
      << *capability << ", expected: " << *original_capability;
}

TEST_F(ExtensionPrinterHandlerTest, GetCapability_Reset) {
  size_t call_count = 0;
  std::unique_ptr<base::DictionaryValue> capability;

  extension_printer_handler_->StartGetCapability(
      kPrinterId, base::Bind(&RecordCapability, &call_count, &capability));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_capability_count());

  extension_printer_handler_->Reset();

  std::string error;
  std::unique_ptr<base::DictionaryValue> original_capability(
      GetJSONAsDictionaryValue(kPWGRasterOnlyPrinterSimpleDescription, &error));
  ASSERT_TRUE(original_capability)
      << "Error deserializing capability: " << error;

  fake_api->TriggerNextGetCapabilityCallback(*original_capability);

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pdf) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data = base::MakeRefCounted<base::RefCountedStaticMemory>(
      kPrintData, kPrintDataLength);
  base::string16 title = base::ASCIIToUTF16("Title");

  extension_printer_handler_->StartPrint(
      kPrinterId, kPdfSupportedPrinter, title, kEmptyPrintTicket,
      gfx::Size(100, 100), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  const PrinterProviderPrintJob* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(title, print_job->job_title);
  EXPECT_EQ(kEmptyPrintTicket, print_job->ticket_json);
  EXPECT_EQ(kContentTypePDF, print_job->content_type);
  EXPECT_TRUE(print_job->document_path.empty());
  ASSERT_TRUE(print_job->document_bytes);
  EXPECT_EQ(RefCountedMemoryToString(print_data),
            RefCountedMemoryToString(print_job->document_bytes));

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pdf_Reset) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data =
      base::MakeRefCounted<base::RefCountedBytes>(kPrintData, kPrintDataLength);
  base::string16 title = base::ASCIIToUTF16("Title");

  extension_printer_handler_->StartPrint(
      kPrinterId, kPdfSupportedPrinter, title, kEmptyPrintTicket,
      gfx::Size(100, 100), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  extension_printer_handler_->Reset();

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, Print_All) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data =
      base::MakeRefCounted<base::RefCountedBytes>(kPrintData, kPrintDataLength);
  base::string16 title = base::ASCIIToUTF16("Title");

  extension_printer_handler_->StartPrint(
      kPrinterId, kAllContentTypesSupportedPrinter, title, kEmptyPrintTicket,
      gfx::Size(100, 100), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  const PrinterProviderPrintJob* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(title, print_job->job_title);
  EXPECT_EQ(kEmptyPrintTicket, print_job->ticket_json);
  EXPECT_EQ(kContentTypePDF, print_job->content_type);
  EXPECT_TRUE(print_job->document_path.empty());
  ASSERT_TRUE(print_job->document_bytes);
  EXPECT_EQ(RefCountedMemoryToString(print_data),
            RefCountedMemoryToString(print_job->document_bytes));

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pwg) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data =
      base::MakeRefCounted<base::RefCountedBytes>(kPrintData, kPrintDataLength);
  base::string16 title = base::ASCIIToUTF16("Title");

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinterSimpleDescription, title,
      kEmptyPrintTicket, gfx::Size(100, 50), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  content::RunAllTasksUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  EXPECT_EQ(printing::TRANSFORM_NORMAL,
            pwg_raster_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_raster_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_FALSE(pwg_raster_converter_->bitmap_settings().reverse_page_order);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().use_color);

  EXPECT_EQ(gfx::Size(printing::kDefaultPdfDpi, printing::kDefaultPdfDpi),
            pwg_raster_converter_->conversion_settings().dpi);
  EXPECT_TRUE(pwg_raster_converter_->conversion_settings().autorotate);
  // size = vertically_oriented_size * vertical_dpi / points_per_inch x
  //        horizontally_oriented_size * horizontal_dpi / points_per_inch
  EXPECT_EQ("0,0 208x416",
            pwg_raster_converter_->conversion_settings().area.ToString());

  const PrinterProviderPrintJob* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(title, print_job->job_title);
  EXPECT_EQ(kEmptyPrintTicket, print_job->ticket_json);
  EXPECT_EQ(kContentTypePWG, print_job->content_type);
  EXPECT_FALSE(print_job->document_bytes);
  EXPECT_FALSE(print_job->document_path.empty());
  EXPECT_EQ(pwg_raster_converter_->path(), print_job->document_path);
  EXPECT_EQ(static_cast<int64_t>(print_data->size()),
            print_job->file_info.size);

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pwg_NonDefaultSettings) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data =
      base::MakeRefCounted<base::RefCountedBytes>(kPrintData, kPrintDataLength);
  base::string16 title = base::ASCIIToUTF16("Title");

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinter, title, kPrintTicketWithDuplex,
      gfx::Size(100, 50), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  content::RunAllTasksUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  EXPECT_EQ(printing::TRANSFORM_FLIP_VERTICAL,
            pwg_raster_converter_->bitmap_settings().odd_page_transform);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().reverse_page_order);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().use_color);

  EXPECT_EQ(gfx::Size(200, 100),
            pwg_raster_converter_->conversion_settings().dpi);
  EXPECT_TRUE(pwg_raster_converter_->conversion_settings().autorotate);
  // size = vertically_oriented_size * vertical_dpi / points_per_inch x
  //        horizontally_oriented_size * horizontal_dpi / points_per_inch
  EXPECT_EQ("0,0 138x138",
            pwg_raster_converter_->conversion_settings().area.ToString());

  const PrinterProviderPrintJob* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(title, print_job->job_title);
  EXPECT_EQ(kPrintTicketWithDuplex, print_job->ticket_json);
  EXPECT_EQ(kContentTypePWG, print_job->content_type);
  EXPECT_FALSE(print_job->document_bytes);
  EXPECT_FALSE(print_job->document_path.empty());
  EXPECT_EQ(pwg_raster_converter_->path(), print_job->document_path);
  EXPECT_EQ(static_cast<int64_t>(print_data->size()),
            print_job->file_info.size);

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pwg_Reset) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data =
      base::MakeRefCounted<base::RefCountedBytes>(kPrintData, kPrintDataLength);
  base::string16 title = base::ASCIIToUTF16("Title");

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinterSimpleDescription, title,
      kEmptyPrintTicket, gfx::Size(100, 50), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  content::RunAllTasksUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  extension_printer_handler_->Reset();

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pwg_InvalidTicket) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data =
      base::MakeRefCounted<base::RefCountedBytes>(kPrintData, kPrintDataLength);
  base::string16 title = base::ASCIIToUTF16("Title");

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinterSimpleDescription, title,
      "{}" /* ticket */, gfx::Size(100, 100), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(1u, call_count);

  EXPECT_FALSE(success);
  EXPECT_EQ("INVALID_TICKET", status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pwg_FailedConversion) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  pwg_raster_converter_->FailConversion();

  auto print_data =
      base::MakeRefCounted<base::RefCountedBytes>(kPrintData, kPrintDataLength);
  base::string16 title = base::ASCIIToUTF16("Title");

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinterSimpleDescription, title,
      kEmptyPrintTicket, gfx::Size(100, 100), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(1u, call_count);

  EXPECT_FALSE(success);
  EXPECT_EQ("INVALID_DATA", status);
}

TEST_F(ExtensionPrinterHandlerTest, GrantUsbPrinterAccess) {
  auto device =
      base::MakeRefCounted<MockUsbDevice>(0, 0, "Google", "USB Printer", "");
  usb_service().AddDevice(device);

  size_t call_count = 0;
  std::unique_ptr<base::DictionaryValue> printer_info;

  std::string printer_id = base::StringPrintf(
      "provisional-usb:fake extension id:%s", device->guid().c_str());
  extension_printer_handler_->StartGrantPrinterAccess(
      printer_id, base::Bind(&RecordPrinterInfo, &call_count, &printer_info));

  EXPECT_FALSE(printer_info.get());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_usb_info_count());

  std::unique_ptr<base::DictionaryValue> original_printer_info(
      DictionaryBuilder()
          .Set("id", "printer1")
          .Set("name", "Printer 1")
          .Build());

  fake_api->TriggerNextUsbPrinterInfoCallback(*original_printer_info);

  EXPECT_EQ(1u, call_count);
  ASSERT_TRUE(printer_info.get());
  EXPECT_TRUE(printer_info->Equals(original_printer_info.get()))
      << *printer_info << ", expected: " << *original_printer_info;
}

TEST_F(ExtensionPrinterHandlerTest, GrantUsbPrinterAccess_Reset) {
  auto device =
      base::MakeRefCounted<MockUsbDevice>(0, 0, "Google", "USB Printer", "");
  usb_service().AddDevice(device);

  size_t call_count = 0;
  std::unique_ptr<base::DictionaryValue> printer_info;

  extension_printer_handler_->StartGrantPrinterAccess(
      base::StringPrintf("provisional-usb:fake extension id:%s",
                         device->guid().c_str()),
      base::Bind(&RecordPrinterInfo, &call_count, &printer_info));

  EXPECT_FALSE(printer_info.get());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_usb_info_count());

  extension_printer_handler_->Reset();

  std::unique_ptr<base::DictionaryValue> original_printer_info(
      DictionaryBuilder()
          .Set("id", "printer1")
          .Set("name", "Printer 1")
          .Build());

  fake_api->TriggerNextUsbPrinterInfoCallback(*original_printer_info);

  EXPECT_EQ(0u, call_count);
  EXPECT_FALSE(printer_info.get());
}
