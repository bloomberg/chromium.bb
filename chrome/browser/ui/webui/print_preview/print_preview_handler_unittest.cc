// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

const char kDummyPrinterName[] = "DefaultPrinter";
const char kDummyInitiatorName[] = "TestInitiator";
const char kTestData[] = "abc";

// Array of all printing::PrinterType values.
const printing::PrinterType kAllTypes[] = {
    printing::kPrivetPrinter, printing::kExtensionPrinter,
    printing::kPdfPrinter, printing::kLocalPrinter};

struct PrinterInfo {
  std::string id;
  bool is_default;
  base::Value basic_info = base::Value(base::Value::Type::DICTIONARY);
  base::Value capabilities = base::Value(base::Value::Type::DICTIONARY);
};

PrinterInfo GetSimplePrinterInfo(const std::string& name, bool is_default) {
  PrinterInfo simple_printer;
  simple_printer.id = name;
  simple_printer.is_default = is_default;
  simple_printer.basic_info.SetKey("printer_name",
                                   base::Value(simple_printer.id));
  simple_printer.basic_info.SetKey("printer_description",
                                   base::Value("Printer for test"));
  simple_printer.basic_info.SetKey("printer_status", base::Value(1));
  base::Value cdd(base::Value::Type::DICTIONARY);
  base::Value capabilities(base::Value::Type::DICTIONARY);
  simple_printer.capabilities.SetKey("printer",
                                     simple_printer.basic_info.Clone());
  simple_printer.capabilities.SetKey("capabilities", cdd.Clone());
  return simple_printer;
}

PrinterInfo GetEmptyPrinterInfo() {
  PrinterInfo empty_printer;
  empty_printer.id = "EmptyPrinter";
  empty_printer.is_default = false;
  empty_printer.basic_info.SetKey("printer_name",
                                  base::Value(empty_printer.id));
  empty_printer.basic_info.SetKey("printer_description",
                                  base::Value("Printer with no capabilities"));
  empty_printer.basic_info.SetKey("printer_status", base::Value(0));
  empty_printer.capabilities.SetKey("printer",
                                    empty_printer.basic_info.Clone());
  return empty_printer;
}

// Creates a print ticket with some default values. Based on ticket creation in
// chrome/browser/resources/print_preview/native_layer.js.
base::Value GetPrintTicket(printing::PrinterType type, bool cloud) {
  bool is_privet_printer = !cloud && type == printing::kPrivetPrinter;
  bool is_extension_printer = !cloud && type == printing::kExtensionPrinter;
  base::Value::DictStorage ticket;

  // Letter
  base::Value::DictStorage media_size;
  media_size["is_default"] = std::make_unique<base::Value>(true);
  media_size["width_microns"] = std::make_unique<base::Value>(215900);
  media_size["height_microns"] = std::make_unique<base::Value>(279400);
  ticket["mediaSize"] = std::make_unique<base::Value>(media_size);

  ticket["pageCount"] = std::make_unique<base::Value>(1);
  ticket["landscape"] = std::make_unique<base::Value>(false);
  ticket["color"] = std::make_unique<base::Value>(2);  // color printing
  ticket["headerFooterEnabled"] = std::make_unique<base::Value>(false);
  ticket["marginsType"] = std::make_unique<base::Value>(0);  // default margins
  ticket["duplex"] = std::make_unique<base::Value>(1);       // LONG_EDGE
  ticket["copies"] = std::make_unique<base::Value>(1);
  ticket["collate"] = std::make_unique<base::Value>(false);
  ticket["shouldPrintBackgrounds"] = std::make_unique<base::Value>(false);
  ticket["shouldPrintSelectionOnly"] = std::make_unique<base::Value>(false);
  ticket["previewModifiable"] = std::make_unique<base::Value>(false);
  ticket["printToPDF"] =
      std::make_unique<base::Value>(!cloud && type == printing::kPdfPrinter);
  ticket["printWithCloudPrint"] = std::make_unique<base::Value>(cloud);
  ticket["printWithPrivet"] = std::make_unique<base::Value>(is_privet_printer);
  ticket["printWithExtension"] =
      std::make_unique<base::Value>(is_extension_printer);
  ticket["rasterizePDF"] = std::make_unique<base::Value>(false);
  ticket["scaleFactor"] = std::make_unique<base::Value>(100);
  ticket["dpiHorizontal"] = std::make_unique<base::Value>(600);
  ticket["dpiVertical"] = std::make_unique<base::Value>(600);
  ticket["deviceName"] = std::make_unique<base::Value>(kDummyPrinterName);
  ticket["fitToPageEnabled"] = std::make_unique<base::Value>(true);
  ticket["pageWidth"] = std::make_unique<base::Value>(215900);
  ticket["pageHeight"] = std::make_unique<base::Value>(279400);
  ticket["showSystemDialog"] = std::make_unique<base::Value>(false);

  if (cloud)
    ticket["cloudPrintID"] = std::make_unique<base::Value>(kDummyPrinterName);

  if (is_privet_printer || is_extension_printer) {
    base::Value::DictStorage capabilities;
    capabilities["duplex"] = std::make_unique<base::Value>(true);  // non-empty
    std::string caps_string;
    base::JSONWriter::Write(base::Value(capabilities), &caps_string);
    ticket["capabilities"] = std::make_unique<base::Value>(caps_string);
    base::Value::DictStorage print_ticket;
    print_ticket["version"] = std::make_unique<base::Value>("1.0");
    print_ticket["print"] = std::make_unique<base::Value>();
    std::string ticket_string;
    base::JSONWriter::Write(base::Value(print_ticket), &ticket_string);
    ticket["ticket"] = std::make_unique<base::Value>(ticket_string);
  }

  return base::Value(ticket);
}

class TestPrinterHandler : public PrinterHandler {
 public:
  explicit TestPrinterHandler(const std::vector<PrinterInfo>& printers) {
    SetPrinters(printers);
  }

  ~TestPrinterHandler() override {}

  void Reset() override {}

  void GetDefaultPrinter(const DefaultPrinterCallback& cb) override {
    cb.Run(default_printer_);
  }

  void StartGetPrinters(const AddedPrintersCallback& added_printers_callback,
                        const GetPrintersDoneCallback& done_callback) override {
    if (!printers_.empty())
      added_printers_callback.Run(printers_);
    done_callback.Run();
  }

  void StartGetCapability(const std::string& destination_id,
                          const GetCapabilityCallback& callback) override {
    callback.Run(base::DictionaryValue::From(std::make_unique<base::Value>(
        printer_capabilities_[destination_id]->Clone())));
  }

  void StartGrantPrinterAccess(
      const std::string& printer_id,
      const GetPrinterInfoCallback& callback) override {}

  void StartPrint(const std::string& destination_id,
                  const std::string& capability,
                  const base::string16& job_title,
                  const std::string& ticket_json,
                  const gfx::Size& page_size,
                  const scoped_refptr<base::RefCountedBytes>& print_data,
                  const PrintCallback& callback) override {
    callback.Run(base::Value());
  }

  void SetPrinters(const std::vector<PrinterInfo>& printers) {
    base::Value::ListStorage printer_list;
    for (const auto& printer : printers) {
      if (printer.is_default)
        default_printer_ = printer.id;
      printer_list.push_back(printer.basic_info.Clone());
      printer_capabilities_[printer.id] = base::DictionaryValue::From(
          std::make_unique<base::Value>(printer.capabilities.Clone()));
    }
    printers_ = base::ListValue(printer_list);
  }

 private:
  std::string default_printer_;
  base::ListValue printers_;
  std::map<std::string, std::unique_ptr<base::DictionaryValue>>
      printer_capabilities_;

  DISALLOW_COPY_AND_ASSIGN(TestPrinterHandler);
};

class FakePrintPreviewUI : public PrintPreviewUI {
 public:
  FakePrintPreviewUI(content::WebUI* web_ui,
                     std::unique_ptr<PrintPreviewHandler> handler)
      : PrintPreviewUI(web_ui, std::move(handler)) {}

  ~FakePrintPreviewUI() override {}

  void GetPrintPreviewDataForIndex(
      int index,
      scoped_refptr<base::RefCountedBytes>* data) const override {
    *data = base::MakeRefCounted<base::RefCountedBytes>(
        reinterpret_cast<const unsigned char*>(kTestData),
        sizeof(kTestData) - 1);
  }

  int GetAvailableDraftPageCount() const override { return 1; }

  void OnPrintPreviewRequest(int request_id) override {}
  void OnCancelPendingPreviewRequest() override {}
  void OnHidePreviewDialog() override {}
  void OnClosePrintPreviewDialog() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePrintPreviewUI);
};

class TestPrintPreviewHandler : public PrintPreviewHandler {
 public:
  explicit TestPrintPreviewHandler(
      std::unique_ptr<PrinterHandler> printer_handler)
      : test_printer_handler_(std::move(printer_handler)) {}

  PrinterHandler* GetPrinterHandler(PrinterType printer_type) override {
    called_for_type_.insert(printer_type);
    return test_printer_handler_.get();
  }

  void RegisterForGaiaCookieChanges() override {}
  void UnregisterForGaiaCookieChanges() override {}

  bool CalledOnlyForType(PrinterType printer_type) {
    return (called_for_type_.size() == 1 &&
            *called_for_type_.begin() == printer_type);
  }

  bool NotCalled() { return called_for_type_.empty(); }

  void reset_calls() { called_for_type_.clear(); }

 private:
  base::flat_set<PrinterType> called_for_type_;
  std::unique_ptr<PrinterHandler> test_printer_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestPrintPreviewHandler);
};

}  // namespace

}  // namespace printing

class PrintPreviewHandlerTest : public testing::Test {
 public:
  PrintPreviewHandlerTest() {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    preview_web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get())));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(preview_web_contents_.get());

    printers_.push_back(
        printing::GetSimplePrinterInfo(printing::kDummyPrinterName, true));
    auto printer_handler =
        std::make_unique<printing::TestPrinterHandler>(printers_);
    printer_handler_ = printer_handler.get();

    auto preview_handler = std::make_unique<printing::TestPrintPreviewHandler>(
        std::move(printer_handler));
    preview_handler->set_web_ui(web_ui());
    handler_ = preview_handler.get();

    auto preview_ui = std::make_unique<printing::FakePrintPreviewUI>(
        web_ui(), std::move(preview_handler));
    preview_ui->SetInitiatorTitle(
        base::ASCIIToUTF16(printing::kDummyInitiatorName));
    web_ui()->SetController(preview_ui.release());
  }

  void Initialize() {
    // Sending this message will enable javascript, so it must always be called
    // before any other messages are sent.
    base::ListValue list_args;
    list_args.AppendString("test-callback-id-0");
    handler()->HandleGetInitialSettings(&list_args);

    // In response to get initial settings, the initial settings are sent back
    // and a use-cloud-print event is dispatched.
    ASSERT_EQ(2u, web_ui()->call_data().size());
  }

  void AssertWebUIEventFired(const content::TestWebUI::CallData& data,
                             const std::string& event_id) {
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    std::string event_fired;
    ASSERT_TRUE(data.arg1()->GetAsString(&event_fired));
    EXPECT_EQ(event_id, event_fired);
  }

  void CheckWebUIResponse(const content::TestWebUI::CallData& data,
                          const std::string& callback_id_in,
                          bool expect_success) {
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(callback_id_in, callback_id);
    bool success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
    EXPECT_EQ(expect_success, success);
  }

  // Validates the initial settings structure in the response matches the
  // print_preview.NativeInitialSettings type in
  // chrome/browser/resources/print_preview/native_layer.js. Checks that
  // |default_printer_name| is the printer name returned and that
  // |initiator_title| is the initiator title returned. Assumes
  // "test-callback-id-0" was used as the callback id.
  void ValidateInitialSettings(const content::TestWebUI::CallData& data,
                               const std::string& default_printer_name,
                               const std::string& initiator_title) {
    CheckWebUIResponse(data, "test-callback-id-0", true);
    const base::Value* settings = data.arg3();
    ASSERT_TRUE(settings->FindPathOfType({"isInKioskAutoPrintMode"},
                                         base::Value::Type::BOOLEAN));
    ASSERT_TRUE(settings->FindPathOfType({"isInAppKioskMode"},
                                         base::Value::Type::BOOLEAN));
    ASSERT_TRUE(settings->FindPathOfType({"thousandsDelimeter"},
                                         base::Value::Type::STRING));
    ASSERT_TRUE(settings->FindPathOfType({"decimalDelimeter"},
                                         base::Value::Type::STRING));
    ASSERT_TRUE(
        settings->FindPathOfType({"unitType"}, base::Value::Type::INTEGER));
    ASSERT_TRUE(settings->FindPathOfType({"previewModifiable"},
                                         base::Value::Type::BOOLEAN));
    const base::Value* title =
        settings->FindPathOfType({"documentTitle"}, base::Value::Type::STRING);
    ASSERT_TRUE(title);
    EXPECT_EQ(initiator_title, title->GetString());
    ASSERT_TRUE(settings->FindPathOfType({"documentHasSelection"},
                                         base::Value::Type::BOOLEAN));
    ASSERT_TRUE(settings->FindPathOfType({"shouldPrintSelectionOnly"},
                                         base::Value::Type::BOOLEAN));
    const base::Value* printer =
        settings->FindPathOfType({"printerName"}, base::Value::Type::STRING);
    ASSERT_TRUE(printer);
    EXPECT_EQ(default_printer_name, printer->GetString());
  }

  const Profile* profile() { return profile_.get(); }
  content::TestWebUI* web_ui() { return web_ui_.get(); }
  printing::TestPrintPreviewHandler* handler() { return handler_; }
  printing::TestPrinterHandler* printer_handler() { return printer_handler_; }
  std::vector<printing::PrinterInfo>& printers() { return printers_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<content::WebContents> preview_web_contents_;
  std::vector<printing::PrinterInfo> printers_;
  printing::TestPrinterHandler* printer_handler_;
  printing::TestPrintPreviewHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandlerTest);
};

TEST_F(PrintPreviewHandlerTest, InitialSettings) {
  Initialize();

  // Verify initial settings were sent.
  ValidateInitialSettings(*web_ui()->call_data().back(),
                          printing::kDummyPrinterName,
                          printing::kDummyInitiatorName);

  // Check that the use-cloud-print event got sent
  AssertWebUIEventFired(*web_ui()->call_data().front(), "use-cloud-print");
}

TEST_F(PrintPreviewHandlerTest, GetPrinters) {
  Initialize();

  // Check all three printer types that implement
  // PrinterHandler::StartGetPrinters().
  const printing::PrinterType types[] = {printing::kPrivetPrinter,
                                         printing::kExtensionPrinter,
                                         printing::kLocalPrinter};
  for (size_t i = 0; i < arraysize(types); i++) {
    printing::PrinterType type = types[i];
    handler()->reset_calls();
    base::ListValue args;
    std::string callback_id_in =
        "test-callback-id-" + base::UintToString(i + 1);
    args.AppendString(callback_id_in);
    args.AppendInteger(type);
    handler()->HandleGetPrinters(&args);
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 2 calls from initial settings, then add 2 more for each loop
    // iteration (one for printers-added, and one for the response).
    ASSERT_EQ(2u + 2 * (i + 1), web_ui()->call_data().size());

    // Validate printers-added
    const content::TestWebUI::CallData& add_data =
        *web_ui()->call_data()[web_ui()->call_data().size() - 2];
    AssertWebUIEventFired(add_data, "printers-added");
    int type_out;
    ASSERT_TRUE(add_data.arg2()->GetAsInteger(&type_out));
    EXPECT_EQ(type, type_out);
    ASSERT_TRUE(add_data.arg3());
    const base::Value::ListStorage& printer_list = add_data.arg3()->GetList();
    ASSERT_EQ(printer_list.size(), 1u);
    EXPECT_TRUE(printer_list[0].FindPathOfType({"printer_name"},
                                               base::Value::Type::STRING));

    // Verify getPrinters promise was resolved successfully.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, true);
  }
}

TEST_F(PrintPreviewHandlerTest, GetPrinterCapabilities) {
  // Add an empty printer to the handler.
  printers().push_back(printing::GetEmptyPrinterInfo());
  printer_handler()->SetPrinters(printers());

  // Initial settings first to enable javascript.
  Initialize();

  // Check all four printer types that implement
  // PrinterHandler::StartGetCapability().
  for (size_t i = 0; i < arraysize(printing::kAllTypes); i++) {
    printing::PrinterType type = printing::kAllTypes[i];
    handler()->reset_calls();
    base::ListValue args;
    std::string callback_id_in =
        "test-callback-id-" + base::UintToString(i + 1);
    args.AppendString(callback_id_in);
    args.AppendString(printing::kDummyPrinterName);
    args.AppendInteger(type);
    handler()->HandleGetPrinterCapabilities(&args);
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 2 calls from initial settings, then add 1 more for each loop
    // iteration.
    ASSERT_EQ(2u + (i + 1), web_ui()->call_data().size());

    // Verify that the printer capabilities promise was resolved correctly.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, true);
    const base::Value* settings = data.arg3();
    ASSERT_TRUE(settings);
    EXPECT_TRUE(settings->FindPathOfType({printing::kSettingCapabilities},
                                         base::Value::Type::DICTIONARY));
  }

  // Run through the loop again, this time with a printer that has no
  // capabilities.
  for (size_t i = 0; i < arraysize(printing::kAllTypes); i++) {
    printing::PrinterType type = printing::kAllTypes[i];
    handler()->reset_calls();
    base::ListValue args;
    std::string callback_id_in =
        "test-callback-id-" +
        base::UintToString(i + arraysize(printing::kAllTypes) + 1);
    args.AppendString(callback_id_in);
    args.AppendString("EmptyPrinter");
    args.AppendInteger(type);
    handler()->HandleGetPrinterCapabilities(&args);
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 2 calls from initial settings plus
    // arraysize(printing::kAllTypes) from first loop, then add 1 more for each
    // loop iteration.
    ASSERT_EQ(2u + arraysize(printing::kAllTypes) + (i + 1),
              web_ui()->call_data().size());

    // Verify printer capabilities promise was rejected.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, false);
  }
}

TEST_F(PrintPreviewHandlerTest, Print) {
  Initialize();

  // All four printer types can print, as well as cloud printers.
  for (size_t i = 0; i <= arraysize(printing::kAllTypes); i++) {
    // Also check cloud print. Use dummy type value of Privet (will be ignored).
    bool cloud = i == arraysize(printing::kAllTypes);
    printing::PrinterType type =
        cloud ? printing::kPrivetPrinter : printing::kAllTypes[i];
    handler()->reset_calls();
    base::ListValue args;
    std::string callback_id_in =
        "test-callback-id-" + base::UintToString(i + 1);
    args.AppendString(callback_id_in);
    base::Value print_ticket = printing::GetPrintTicket(type, cloud);
    std::string json;
    base::JSONWriter::Write(print_ticket, &json);
    args.AppendString(json);
    handler()->HandlePrint(&args);

    // Verify correct PrinterHandler was called or that no handler was requested
    // for local and cloud printers.
    if (cloud || type == printing::kLocalPrinter) {
      EXPECT_TRUE(handler()->NotCalled());
    } else {
      EXPECT_TRUE(handler()->CalledOnlyForType(type));
    }

    // Verify print promise was resolved successfully.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, true);

    // For cloud print, should also get the encoded data back as a string.
    if (cloud) {
      std::string print_data;
      ASSERT_TRUE(data.arg3()->GetAsString(&print_data));
      std::string expected_data;
      base::Base64Encode(printing::kTestData, &expected_data);
      EXPECT_EQ(print_data, expected_data);
    }
  }
}
