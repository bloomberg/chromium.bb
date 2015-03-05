// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/pwg_raster_converter.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using extensions::PrinterProviderAPI;
using extensions::PrinterProviderPrintJob;
using local_discovery::PWGRasterConverter;

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

// Suffix appended to document data by fake PWGRasterConverter.
const char kPWGConversionSuffix[] = "_converted";

const char kContentTypePDF[] = "application/pdf";
const char kContentTypePWG[] = "image/pwg-raster";

// Print request status considered to be successful by fake PrinterProviderAPI.
const char kPrintRequestSuccess[] = "OK";

// Used as a callback to StartGetPrinters in tests.
// Increases |*call_count| and records values returned by StartGetPrinters.
void RecordPrinterList(size_t* call_count,
                       scoped_ptr<base::ListValue>* printers_out,
                       bool* is_done_out,
                       const base::ListValue& printers,
                       bool is_done) {
  ++(*call_count);
  printers_out->reset(printers.DeepCopy());
  *is_done_out = is_done;
}

// Used as a callback to StartGetCapability in tests.
// Increases |*call_count| and records values returned by StartGetCapability.
void RecordCapability(size_t* call_count,
                      std::string* destination_id_out,
                      scoped_ptr<base::DictionaryValue>* capability_out,
                      const std::string& destination_id,
                      const base::DictionaryValue& capability) {
  ++(*call_count);
  *destination_id_out = destination_id;
  capability_out->reset(capability.DeepCopy());
}

// Used as a callback to StartPrint in tests.
// Increases |*call_count| and records values returned by StartPrint.
void RecordPrintResult(size_t* call_count,
                       bool* success_out,
                       std::string* status_out,
                       bool success,
                       const std::string& status) {
  ++(*call_count);
  *success_out = success;
  *status_out = status;
}

// Converts JSON string to base::ListValue object.
// On failure, returns NULL and fills |*error| string.
scoped_ptr<base::ListValue> GetJSONAsListValue(const std::string& json,
                                               std::string* error) {
  scoped_ptr<base::Value> deserialized(
      JSONStringValueDeserializer(json).Deserialize(NULL, error));
  if (!deserialized)
    return scoped_ptr<base::ListValue>();
  base::ListValue* list = nullptr;
  if (!deserialized->GetAsList(&list)) {
    *error = "Value is not a list.";
    return scoped_ptr<base::ListValue>();
  }
  return scoped_ptr<base::ListValue>(list->DeepCopy());
}

// Converts JSON string to base::DictionaryValue object.
// On failure, returns NULL and fills |*error| string.
scoped_ptr<base::DictionaryValue> GetJSONAsDictionaryValue(
    const std::string& json,
    std::string* error) {
  scoped_ptr<base::Value> deserialized(
      JSONStringValueDeserializer(json).Deserialize(NULL, error));
  if (!deserialized)
    return scoped_ptr<base::DictionaryValue>();
  base::DictionaryValue* dictionary;
  if (!deserialized->GetAsDictionary(&dictionary)) {
    *error = "Value is not a dictionary.";
    return scoped_ptr<base::DictionaryValue>();
  }
  return scoped_ptr<base::DictionaryValue>(dictionary->DeepCopy());
}

// Fake PWGRasterconverter used in the tests.
class FakePWGRasterConverter : public PWGRasterConverter {
 public:
  FakePWGRasterConverter() : fail_conversion_(false), initialized_(false) {}
  ~FakePWGRasterConverter() override = default;

  // PWGRasterConverter implementation.
  // It writes |data| to a temp file, appending it |kPWGConversionSuffix|.
  // Also, remembers conversion and bitmap settings passed into the method.
  void Start(base::RefCountedMemory* data,
             const printing::PdfRenderSettings& conversion_settings,
             const printing::PwgRasterSettings& bitmap_settings,
             const ResultCallback& callback) override {
    if (fail_conversion_) {
      callback.Run(false, base::FilePath());
      return;
    }

    if (!initialized_ && !temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "Unable to create target dir for cenverter";
      callback.Run(false, base::FilePath());
      return;
    }

    initialized_ = true;

    std::string data_str(data->front_as<char>(), data->size());
    data_str.append(kPWGConversionSuffix);
    base::FilePath target_path = temp_dir_.path().AppendASCII("output.pwg");
    int written = WriteFile(target_path, data_str.c_str(), data_str.size());
    if (written != static_cast<int>(data_str.size())) {
      ADD_FAILURE() << "Failed to write pwg raster file.";
      callback.Run(false, base::FilePath());
      return;
    }

    conversion_settings_ = conversion_settings;
    bitmap_settings_ = bitmap_settings;

    callback.Run(true, target_path);
  }

  // Makes |Start| method always return an error.
  void FailConversion() { fail_conversion_ = true; }

  const printing::PdfRenderSettings& conversion_settings() const {
    return conversion_settings_;
  }

  const printing::PwgRasterSettings& bitmap_settings() const {
    return bitmap_settings_;
  }

 private:
  base::ScopedTempDir temp_dir_;

  printing::PdfRenderSettings conversion_settings_;
  printing::PwgRasterSettings bitmap_settings_;
  bool fail_conversion_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(FakePWGRasterConverter);
};

// Copy of data contained in print job passed to |DispatchPrintRequested|.
struct PrintJobParams {
  std::string printer_id;
  std::string ticket;
  std::string content_type;
  std::string document;
};

// Information about received print requests.
struct PrintRequestInfo {
  PrinterProviderAPI::PrintCallback callback;
  PrintJobParams params;
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
    pending_printers_callbacks_.push_back(callback);
  }

  void DispatchGetCapabilityRequested(
      const std::string& destination_id,
      const PrinterProviderAPI::GetCapabilityCallback& callback) override {
    pending_capability_callbacks_.push_back(base::Bind(callback));
  }

  void DispatchPrintRequested(
      const PrinterProviderPrintJob& job,
      const PrinterProviderAPI::PrintCallback& callback) override {
    PrintRequestInfo request_info;
    request_info.callback = callback;

    request_info.params.printer_id = job.printer_id;
    request_info.params.ticket = job.ticket_json;
    request_info.params.content_type = job.content_type;
    request_info.params.document = std::string(
        job.document_bytes->front_as<char>(), job.document_bytes->size());

    pending_print_requests_.push_back(request_info);
  }

  size_t pending_get_printers_count() const {
    return pending_printers_callbacks_.size();
  }

  void TriggerNextGetPrintersCallback(const base::ListValue& printers,
                                      bool done) {
    ASSERT_GT(pending_get_printers_count(), 0u);
    pending_printers_callbacks_[0].Run(printers, done);
    pending_printers_callbacks_.erase(pending_printers_callbacks_.begin());
  }

  size_t pending_get_capability_count() const {
    return pending_capability_callbacks_.size();
  }

  void TriggerNextGetCapabilityCallback(
      const base::DictionaryValue& description) {
    ASSERT_GT(pending_get_capability_count(), 0u);
    pending_capability_callbacks_[0].Run(description);
    pending_capability_callbacks_.erase(pending_capability_callbacks_.begin());
  }

  size_t pending_print_count() const { return pending_print_requests_.size(); }

  const PrintJobParams* GetNextPendingPrintJob() const {
    EXPECT_GT(pending_print_count(), 0u);
    if (pending_print_count() == 0)
      return NULL;
    return &pending_print_requests_[0].params;
  }

  void TriggerNextPrintCallback(const std::string& result) {
    ASSERT_GT(pending_print_count(), 0u);
    pending_print_requests_[0].callback.Run(result == kPrintRequestSuccess,
                                            result);
    pending_print_requests_.erase(pending_print_requests_.begin());
  }

 private:
  std::vector<PrinterProviderAPI::GetPrintersCallback>
      pending_printers_callbacks_;
  std::vector<PrinterProviderAPI::GetCapabilityCallback>
      pending_capability_callbacks_;
  std::vector<PrintRequestInfo> pending_print_requests_;

  DISALLOW_COPY_AND_ASSIGN(FakePrinterProviderAPI);
};

KeyedService* BuildTestingPrinterProviderAPI(content::BrowserContext* context) {
  return new FakePrinterProviderAPI();
}

}  // namespace

class ExtensionPrinterHandlerTest : public testing::Test {
 public:
  ExtensionPrinterHandlerTest() : pwg_raster_converter_(NULL) {}
  ~ExtensionPrinterHandlerTest() override = default;

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        extensions::PrinterProviderAPIFactory::GetInstance(),
        &BuildTestingPrinterProviderAPI);
    profile_ = profile_builder.Build();

    extension_printer_handler_.reset(new ExtensionPrinterHandler(
        profile_.get(), message_loop_.task_runner()));

    pwg_raster_converter_ = new FakePWGRasterConverter();
    extension_printer_handler_->SetPwgRasterConverterForTesting(
        scoped_ptr<PWGRasterConverter>(pwg_raster_converter_));
  }

 protected:
  FakePrinterProviderAPI* GetPrinterProviderAPI() {
    return static_cast<FakePrinterProviderAPI*>(
        extensions::PrinterProviderAPIFactory::GetInstance()
            ->GetForBrowserContext(profile_.get()));
  }

  scoped_ptr<ExtensionPrinterHandler> extension_printer_handler_;

  FakePWGRasterConverter* pwg_raster_converter_;

 private:
  base::MessageLoop message_loop_;

  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPrinterHandlerTest);
};

TEST_F(ExtensionPrinterHandlerTest, GetPrinters) {
  size_t call_count = 0;
  scoped_ptr<base::ListValue> printers;
  bool is_done = false;

  extension_printer_handler_->StartGetPrinters(
      base::Bind(&RecordPrinterList, &call_count, &printers, &is_done));

  EXPECT_FALSE(printers.get());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_printers_count());

  std::string error;
  scoped_ptr<base::ListValue> original_printers(
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
  scoped_ptr<base::ListValue> printers;
  bool is_done = false;

  extension_printer_handler_->StartGetPrinters(
      base::Bind(&RecordPrinterList, &call_count, &printers, &is_done));

  EXPECT_FALSE(printers.get());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_printers_count());

  extension_printer_handler_->Reset();

  std::string error;
  scoped_ptr<base::ListValue> original_printers(
      GetJSONAsListValue(kPrinterDescriptionList, &error));
  ASSERT_TRUE(original_printers) << "Error deserializing printers: " << error;

  fake_api->TriggerNextGetPrintersCallback(*original_printers, true);

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, GetCapability) {
  size_t call_count = 0;
  std::string destination_id;
  scoped_ptr<base::DictionaryValue> capability;

  extension_printer_handler_->StartGetCapability(
      kPrinterId,
      base::Bind(&RecordCapability, &call_count, &destination_id, &capability));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_capability_count());

  std::string error;
  scoped_ptr<base::DictionaryValue> original_capability(
      GetJSONAsDictionaryValue(kPWGRasterOnlyPrinterSimpleDescription, &error));
  ASSERT_TRUE(original_capability)
      << "Error deserializing capability: " << error;

  fake_api->TriggerNextGetCapabilityCallback(*original_capability);

  EXPECT_EQ(1u, call_count);
  EXPECT_EQ(kPrinterId, destination_id);
  ASSERT_TRUE(capability.get());
  EXPECT_TRUE(capability->Equals(original_capability.get()))
      << *capability << ", expected: " << *original_capability;
}

TEST_F(ExtensionPrinterHandlerTest, GetCapability_Reset) {
  size_t call_count = 0;
  std::string destination_id;
  scoped_ptr<base::DictionaryValue> capability;

  extension_printer_handler_->StartGetCapability(
      kPrinterId,
      base::Bind(&RecordCapability, &call_count, &destination_id, &capability));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_capability_count());

  extension_printer_handler_->Reset();

  std::string error;
  scoped_ptr<base::DictionaryValue> original_capability(
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

  scoped_refptr<base::RefCountedString> print_data(
      new base::RefCountedString());
  print_data->data() = "print data, PDF";

  extension_printer_handler_->StartPrint(
      kPrinterId, kPdfSupportedPrinter, kEmptyPrintTicket, gfx::Size(100, 100),
      print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  const PrintJobParams* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(kEmptyPrintTicket, print_job->ticket);
  EXPECT_EQ(kContentTypePDF, print_job->content_type);
  EXPECT_EQ(print_data->data(), print_job->document);

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pdf_Reset) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  scoped_refptr<base::RefCountedString> print_data(
      new base::RefCountedString());
  print_data->data() = "print data, PDF";

  extension_printer_handler_->StartPrint(
      kPrinterId, kPdfSupportedPrinter, kEmptyPrintTicket, gfx::Size(100, 100),
      print_data,
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

  scoped_refptr<base::RefCountedString> print_data(
      new base::RefCountedString());
  print_data->data() = "print data, PDF";

  extension_printer_handler_->StartPrint(
      kPrinterId, kAllContentTypesSupportedPrinter, kEmptyPrintTicket,
      gfx::Size(100, 100), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  const PrintJobParams* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(kEmptyPrintTicket, print_job->ticket);
  EXPECT_EQ(kContentTypePDF, print_job->content_type);
  EXPECT_EQ(print_data->data(), print_job->document);

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pwg) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  scoped_refptr<base::RefCountedString> print_data(
      new base::RefCountedString());
  print_data->data() = "print data, PDF";

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinterSimpleDescription, kEmptyPrintTicket,
      gfx::Size(100, 50), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  base::RunLoop().RunUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  EXPECT_EQ(printing::TRANSFORM_NORMAL,
            pwg_raster_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_raster_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_FALSE(pwg_raster_converter_->bitmap_settings().reverse_page_order);

  EXPECT_EQ(printing::kDefaultPdfDpi,
            pwg_raster_converter_->conversion_settings().dpi());
  EXPECT_TRUE(pwg_raster_converter_->conversion_settings().autorotate());
  EXPECT_EQ("0,0 208x416",  // vertically_oriented_size  * dpi / points_per_inch
            pwg_raster_converter_->conversion_settings().area().ToString());

  const PrintJobParams* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(kEmptyPrintTicket, print_job->ticket);
  EXPECT_EQ(kContentTypePWG, print_job->content_type);
  EXPECT_EQ(print_data->data() + kPWGConversionSuffix, print_job->document);

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pwg_NonDefaultSettings) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  scoped_refptr<base::RefCountedString> print_data(
      new base::RefCountedString());
  print_data->data() = "print data, PDF";

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinter, kPrintTicketWithDuplex,
      gfx::Size(100, 50), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  base::RunLoop().RunUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  EXPECT_EQ(printing::TRANSFORM_FLIP_VERTICAL,
            pwg_raster_converter_->bitmap_settings().odd_page_transform);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().reverse_page_order);

  EXPECT_EQ(200,  // max(vertical_dpi, horizontal_dpi)
            pwg_raster_converter_->conversion_settings().dpi());
  EXPECT_TRUE(pwg_raster_converter_->conversion_settings().autorotate());
  EXPECT_EQ("0,0 138x277",  // vertically_oriented_size  * dpi / points_per_inch
            pwg_raster_converter_->conversion_settings().area().ToString());

  const PrintJobParams* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(kPrintTicketWithDuplex, print_job->ticket);
  EXPECT_EQ(kContentTypePWG, print_job->content_type);
  EXPECT_EQ(print_data->data() + kPWGConversionSuffix, print_job->document);

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, Print_Pwg_Reset) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  scoped_refptr<base::RefCountedString> print_data(
      new base::RefCountedString());
  print_data->data() = "print data, PDF";

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinterSimpleDescription, kEmptyPrintTicket,
      gfx::Size(100, 50), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  base::RunLoop().RunUntilIdle();

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

  scoped_refptr<base::RefCountedString> print_data(
      new base::RefCountedString());
  print_data->data() = "print data, PDF";

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinterSimpleDescription, "{}" /* ticket */,
      gfx::Size(100, 100), print_data,
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

  scoped_refptr<base::RefCountedString> print_data(
      new base::RefCountedString());
  print_data->data() = "print data, PDF";

  extension_printer_handler_->StartPrint(
      kPrinterId, kPWGRasterOnlyPrinterSimpleDescription, kEmptyPrintTicket,
      gfx::Size(100, 100), print_data,
      base::Bind(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(1u, call_count);

  EXPECT_FALSE(success);
  EXPECT_EQ("INVALID_DATA", status);
}
