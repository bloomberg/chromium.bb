// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_message_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_provider.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace printing {

namespace {

// Name of the fake server we're resolving ppds from.
const char kPpdServer[] = "bogus.google.com";

class PpdProviderTest : public ::testing::Test {
 public:
  PpdProviderTest()
      : loop_(base::MessageLoop::TYPE_IO),
        request_context_getter_(new net::TestURLRequestContextGetter(
            base::MessageLoop::current()->task_runner())) {}

  void SetUp() override {
    ASSERT_TRUE(ppd_cache_temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { StopFakePpdServer(); }

  // Create and return a provider for a test that uses the given |locale|.
  scoped_refptr<PpdProvider> CreateProvider(const std::string& locale) {
    auto provider_options = PpdProvider::Options();
    provider_options.ppd_server_root = std::string("https://") + kPpdServer;

    return PpdProvider::Create(
        locale, request_context_getter_.get(),
        PpdCache::Create(ppd_cache_temp_dir_.GetPath(),
                         base::MessageLoop::current()->task_runner()),
        loop_.task_runner().get(), provider_options);
  }

  // Create an interceptor that serves a small fileset of ppd server files.
  void StartFakePpdServer() {
    ASSERT_TRUE(interceptor_temp_dir_.CreateUniqueTempDir());
    interceptor_ = base::MakeUnique<net::TestURLRequestInterceptor>(
        "https", kPpdServer, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
    // Use brace initialization to express the desired server contents as "url",
    // "contents" pairs.
    std::vector<std::pair<std::string, std::string>> server_contents = {
        {"metadata/locales.json",
         R"(["en",
             "es-mx",
             "en-gb"])"},
        {"metadata/index.json",
         R"([
             ["printer_a_ref", "printer_a.ppd"],
             ["printer_b_ref", "printer_b.ppd"],
             ["printer_c_ref", "printer_c.ppd"]
            ])"},
        {"metadata/usb-031f.json",
         R"([
             [1592, "Some canonical reference"],
             [6535, "Some other canonical reference"]
            ])"},
        {"metadata/manufacturers-en.json",
         R"([
            ["manufacturer_a_en", "manufacturer_a.json"],
            ["manufacturer_b_en", "manufacturer_b.json"]
            ])"},
        {"metadata/manufacturers-en-gb.json",
         R"([
            ["manufacturer_a_en-gb", "manufacturer_a.json"],
            ["manufacturer_b_en-gb", "manufacturer_b.json"]
            ])"},
        {"metadata/manufacturers-es-mx.json",
         R"([
            ["manufacturer_a_es-mx", "manufacturer_a.json"],
            ["manufacturer_b_es-mx", "manufacturer_b.json"]
            ])"},
        {"metadata/manufacturer_a.json",
         R"([
            ["printer_a", "printer_a_ref"],
            ["printer_b", "printer_b_ref"]
            ])"},
        {"metadata/manufacturer_b.json",
         R"([
            ["printer_c", "printer_c_ref"]
            ])"},
        {"ppds/printer_a.ppd", "a"},
        {"ppds/printer_b.ppd", "b"},
        {"ppds/printer_c.ppd", "c"},
        {"user_supplied_ppd_directory/user_supplied.ppd", "u"}};
    int next_file_num = 0;
    for (const auto& entry : server_contents) {
      base::FilePath filename = interceptor_temp_dir_.GetPath().Append(
          base::StringPrintf("%d.json", next_file_num++));
      ASSERT_EQ(
          base::WriteFile(filename, entry.second.data(), entry.second.size()),
          static_cast<int>(entry.second.size()))
          << "Failed to write temp server file";
      interceptor_->SetResponse(
          GURL(base::StringPrintf("https://%s/%s", kPpdServer,
                                  entry.first.c_str())),
          filename);
    }
  }

  // Interceptor posts a *task* during destruction that actually unregisters
  // things.  So we have to run the message loop post-interceptor-destruction to
  // actually unregister the URLs, otherwise they won't *actually* be
  // unregistered until the next time we invoke the message loop.  Which may be
  // in the middle of the next test.
  //
  // Note this is harmless to call if we haven't started a fake ppd server.
  void StopFakePpdServer() {
    interceptor_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Capture the result of a ResolveManufacturers() call.
  void CaptureResolveManufacturers(PpdProvider::CallbackResultCode code,
                                   const std::vector<std::string>& data) {
    captured_resolve_manufacturers_.push_back({code, data});
  }

  // Capture the result of a ResolvePrinters() call.
  void CaptureResolvePrinters(PpdProvider::CallbackResultCode code,
                              const std::vector<std::string>& data) {
    captured_resolve_printers_.push_back({code, data});
  }

  // Capture the result of a ResolvePpd() call.
  void CaptureResolvePpd(PpdProvider::CallbackResultCode code,
                         const std::string& contents) {
    captured_resolve_ppd_.push_back({code, contents});
  }

  // Capture the result of a ResolveUsbIds() call.
  void CaptureResolveUsbIds(PpdProvider::CallbackResultCode code,
                            const std::string& contents) {
    captured_resolve_usb_ids_.push_back({code, contents});
  }

  // Discard the result of a ResolvePpd() call.
  void DiscardResolvePpd(PpdProvider::CallbackResultCode code,
                         const std::string& contents) {}

 protected:
  // Run a ResolveManufacturers run from the given locale, expect to get
  // results in expected_used_locale.
  void RunLocalizationTest(const std::string& browser_locale,
                           const std::string& expected_used_locale) {
    captured_resolve_manufacturers_.clear();
    auto provider = CreateProvider(browser_locale);
    provider->ResolveManufacturers(base::Bind(
        &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    provider = nullptr;
    ASSERT_EQ(captured_resolve_manufacturers_.size(), 1UL);
    EXPECT_EQ(captured_resolve_manufacturers_[0].first, PpdProvider::SUCCESS);

    const auto& result_vec = captured_resolve_manufacturers_[0].second;

    // It's sufficient to check for one of the expected locale keys to make sure
    // we got the right map.
    EXPECT_FALSE(std::find(result_vec.begin(), result_vec.end(),
                           "manufacturer_a_" + expected_used_locale) ==
                 result_vec.end());
  }

  // Drain tasks both on the loop we use for network/disk activity and the
  // top-level loop that we're using in the test itself.  Unfortunately, even
  // thought the TestURLRequestContextGetter tells the url fetcher to run on the
  // current message loop, some deep backend processes can get put into other
  // loops, which means we can't just trust RunLoop::RunUntilIdle() to drain
  // outstanding work.
  void Drain(const PpdProvider& provider) {
    do {
      base::RunLoop().RunUntilIdle();
    } while (!provider.Idle());
  }

  // Message loop that runs on the current thread.
  base::TestMessageLoop loop_;

  std::vector<
      std::pair<PpdProvider::CallbackResultCode, std::vector<std::string>>>
      captured_resolve_manufacturers_;

  std::vector<
      std::pair<PpdProvider::CallbackResultCode, std::vector<std::string>>>
      captured_resolve_printers_;

  std::vector<std::pair<PpdProvider::CallbackResultCode, std::string>>
      captured_resolve_ppd_;

  std::vector<std::pair<PpdProvider::CallbackResultCode, std::string>>
      captured_resolve_usb_ids_;

  std::unique_ptr<net::TestURLRequestInterceptor> interceptor_;

  base::ScopedTempDir ppd_cache_temp_dir_;
  base::ScopedTempDir interceptor_temp_dir_;

  // Provider to be used in the test.
  scoped_refptr<PpdProvider> ppd_provider_;

  // Misc extra stuff needed for the test environment to function.
  //  base::TestMessageLoop loop_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
};

// Test that we get back manufacturer maps as expected.
TEST_F(PpdProviderTest, ManufacturersFetch) {
  StartFakePpdServer();
  auto provider = CreateProvider("en");
  // Issue two requests at the same time, both should be resolved properly.
  provider->ResolveManufacturers(base::Bind(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  provider->ResolveManufacturers(base::Bind(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  Drain(*provider);
  ASSERT_EQ(2UL, captured_resolve_manufacturers_.size());
  std::vector<std::string> expected_result(
      {"manufacturer_a_en", "manufacturer_b_en"});
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_manufacturers_[0].first);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_manufacturers_[1].first);
  EXPECT_TRUE(captured_resolve_manufacturers_[0].second == expected_result);
  EXPECT_TRUE(captured_resolve_manufacturers_[1].second == expected_result);
}

// Test that we get a reasonable error when we have no server to contact.  Tis
// is almost exactly the same as the above test, we just don't bring up the fake
// server first.
TEST_F(PpdProviderTest, ManufacturersFetchNoServer) {
  auto provider = CreateProvider("en");
  // Issue two requests at the same time, both should be resolved properly.
  provider->ResolveManufacturers(base::Bind(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  provider->ResolveManufacturers(base::Bind(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  Drain(*provider);
  ASSERT_EQ(2UL, captured_resolve_manufacturers_.size());
  EXPECT_EQ(PpdProvider::SERVER_ERROR,
            captured_resolve_manufacturers_[0].first);
  EXPECT_EQ(PpdProvider::SERVER_ERROR,
            captured_resolve_manufacturers_[1].first);
  EXPECT_TRUE(captured_resolve_manufacturers_[0].second.empty());
  EXPECT_TRUE(captured_resolve_manufacturers_[1].second.empty());
}

// Test that we get things in the requested locale, and that fallbacks are sane.
TEST_F(PpdProviderTest, LocalizationAndFallbacks) {
  StartFakePpdServer();
  RunLocalizationTest("en-gb", "en-gb");
  RunLocalizationTest("en-blah", "en");
  RunLocalizationTest("en-gb-foo", "en-gb");
  RunLocalizationTest("es", "es-mx");
  RunLocalizationTest("bogus", "en");
}

// Test successful and unsuccessful usb resolutions.
TEST_F(PpdProviderTest, UsbResolution) {
  StartFakePpdServer();
  auto provider = CreateProvider("en");

  // Should get back "Some canonical reference"
  provider->ResolveUsbIds(0x031f, 1592,
                          base::Bind(&PpdProviderTest::CaptureResolveUsbIds,
                                     base::Unretained(this)));
  // Should get back "Some other canonical reference"
  provider->ResolveUsbIds(0x031f, 6535,
                          base::Bind(&PpdProviderTest::CaptureResolveUsbIds,
                                     base::Unretained(this)));

  // Extant vendor id, nonexistant device id, should get a NOT_FOUND
  provider->ResolveUsbIds(0x031f, 8162,
                          base::Bind(&PpdProviderTest::CaptureResolveUsbIds,
                                     base::Unretained(this)));

  // Nonexistant vendor id, should get a NOT_FOUND in the real world, but
  // the URL interceptor we're using considers all nonexistant files to
  // be effectively CONNECTION REFUSED, so we just check for non-success
  // on this one.
  provider->ResolveUsbIds(1234, 1782,
                          base::Bind(&PpdProviderTest::CaptureResolveUsbIds,
                                     base::Unretained(this)));
  Drain(*provider);

  ASSERT_EQ(captured_resolve_usb_ids_.size(), static_cast<size_t>(4));
  EXPECT_EQ(captured_resolve_usb_ids_[0].first, PpdProvider::SUCCESS);
  EXPECT_EQ(captured_resolve_usb_ids_[0].second, "Some canonical reference");
  EXPECT_EQ(captured_resolve_usb_ids_[1].first, PpdProvider::SUCCESS);
  EXPECT_EQ(captured_resolve_usb_ids_[1].second,
            "Some other canonical reference");
  EXPECT_EQ(captured_resolve_usb_ids_[2].first, PpdProvider::NOT_FOUND);
  EXPECT_EQ(captured_resolve_usb_ids_[2].second, "");
  EXPECT_FALSE(captured_resolve_usb_ids_[3].first == PpdProvider::SUCCESS);
  EXPECT_EQ(captured_resolve_usb_ids_[3].second, "");
}

// For convenience a null ResolveManufacturers callback target.
void ResolveManufacturersNop(PpdProvider::CallbackResultCode code,
                             const std::vector<std::string>& v) {}

// Test basic ResolvePrinters() functionality.  At the same time, make
// sure we can get the PpdReference for each of the resolved printers.
TEST_F(PpdProviderTest, ResolvePrinters) {
  StartFakePpdServer();
  auto provider = CreateProvider("en");

  // Grab the manufacturer list, but don't bother to save it, we know what
  // should be in it and we check that elsewhere.  We just need to run the
  // resolve to populate the internal PpdProvider structures.
  provider->ResolveManufacturers(base::Bind(&ResolveManufacturersNop));
  Drain(*provider);

  provider->ResolvePrinters("manufacturer_a_en",
                            base::Bind(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));
  provider->ResolvePrinters("manufacturer_b_en",
                            base::Bind(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));
  Drain(*provider);
  ASSERT_EQ(2UL, captured_resolve_printers_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_printers_[0].first);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_printers_[1].first);
  EXPECT_EQ(2UL, captured_resolve_printers_[0].second.size());
  EXPECT_EQ(std::vector<std::string>({"printer_a", "printer_b"}),
            captured_resolve_printers_[0].second);
  EXPECT_EQ(std::vector<std::string>({"printer_c"}),
            captured_resolve_printers_[1].second);

  // We have manufacturers and models, we should be able to get a ppd out of
  // this.
  Printer::PpdReference ref;
  ASSERT_TRUE(
      provider->GetPpdReference("manufacturer_a_en", "printer_b", &ref));
}

// Test that if we give a bad reference to ResolvePrinters(), we get an
// INTERNAL_ERROR.
TEST_F(PpdProviderTest, ResolvePrintersBadReference) {
  StartFakePpdServer();
  auto provider = CreateProvider("en");
  provider->ResolveManufacturers(base::Bind(&ResolveManufacturersNop));
  Drain(*provider);

  provider->ResolvePrinters("bogus_doesnt_exist",
                            base::Bind(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));
  Drain(*provider);
  ASSERT_EQ(1UL, captured_resolve_printers_.size());
  EXPECT_EQ(PpdProvider::INTERNAL_ERROR, captured_resolve_printers_[0].first);
}

// Test that if the server is unavailable, we get SERVER_ERRORs back out.
TEST_F(PpdProviderTest, ResolvePrintersNoServer) {
  StartFakePpdServer();
  auto provider = CreateProvider("en");
  provider->ResolveManufacturers(base::Bind(&ResolveManufacturersNop));
  Drain(*provider);

  StopFakePpdServer();

  provider->ResolvePrinters("manufacturer_a_en",
                            base::Bind(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));
  provider->ResolvePrinters("manufacturer_b_en",
                            base::Bind(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));
  Drain(*provider);
  ASSERT_EQ(2UL, captured_resolve_printers_.size());
  EXPECT_EQ(PpdProvider::SERVER_ERROR, captured_resolve_printers_[0].first);
  EXPECT_EQ(PpdProvider::SERVER_ERROR, captured_resolve_printers_[1].first);
}

// Test a successful ppd resolution from an effective_make_and_model reference.
TEST_F(PpdProviderTest, ResolveServerKeyPpd) {
  StartFakePpdServer();
  auto provider = CreateProvider("en");
  Printer::PpdReference ref;
  ref.effective_make_and_model = "printer_b_ref";
  provider->ResolvePpd(ref, base::Bind(&PpdProviderTest::CaptureResolvePpd,
                                       base::Unretained(this)));
  ref.effective_make_and_model = "printer_c_ref";
  provider->ResolvePpd(ref, base::Bind(&PpdProviderTest::CaptureResolvePpd,
                                       base::Unretained(this)));
  Drain(*provider);

  ASSERT_EQ(2UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].first);
  EXPECT_EQ("b", captured_resolve_ppd_[0].second);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[1].first);
  EXPECT_EQ("c", captured_resolve_ppd_[1].second);
}

// Test that we *don't* resolve a ppd URL over non-file schemes.  It's not clear
// whether we'll want to do this in the long term, but for now this is
// disallowed because we're not sure we completely understand the security
// implications.
TEST_F(PpdProviderTest, ResolveUserSuppliedUrlPpdFromNetworkFails) {
  StartFakePpdServer();
  auto provider = CreateProvider("en");

  Printer::PpdReference ref;
  ref.user_supplied_ppd_url = base::StringPrintf(
      "https://%s/user_supplied_ppd_directory/user_supplied.ppd", kPpdServer);
  provider->ResolvePpd(ref, base::Bind(&PpdProviderTest::CaptureResolvePpd,
                                       base::Unretained(this)));
  Drain(*provider);

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::INTERNAL_ERROR, captured_resolve_ppd_[0].first);
  EXPECT_TRUE(captured_resolve_ppd_[0].second.empty());
}

// Test a successful ppd resolution from a user_supplied_url field when
// reading from a file.  Note we shouldn't need the server to be up
// to do this successfully, as we should be able to do this offline.
TEST_F(PpdProviderTest, ResolveUserSuppliedUrlPpdFromFile) {
  auto provider = CreateProvider("en");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath filename = temp_dir.GetPath().Append("my_spiffy.ppd");

  std::string user_ppd_contents = "Woohoo";

  ASSERT_EQ(base::WriteFile(filename, user_ppd_contents.data(),
                            user_ppd_contents.size()),
            static_cast<int>(user_ppd_contents.size()));

  Printer::PpdReference ref;
  ref.user_supplied_ppd_url =
      base::StringPrintf("file://%s", filename.MaybeAsASCII().c_str());
  provider->ResolvePpd(ref, base::Bind(&PpdProviderTest::CaptureResolvePpd,
                                       base::Unretained(this)));
  Drain(*provider);

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].first);
  EXPECT_EQ(user_ppd_contents, captured_resolve_ppd_[0].second);
}

// Test that we cache ppd resolutions when we fetch them and that we can resolve
// from the cache without the server available.
TEST_F(PpdProviderTest, ResolvedPpdsGetCached) {
  auto provider = CreateProvider("en");
  std::string user_ppd_contents = "Woohoo";
  Printer::PpdReference ref;
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath filename = temp_dir.GetPath().Append("my_spiffy.ppd");

    ASSERT_EQ(base::WriteFile(filename, user_ppd_contents.data(),
                              user_ppd_contents.size()),
              static_cast<int>(user_ppd_contents.size()));

    ref.user_supplied_ppd_url =
        base::StringPrintf("file://%s", filename.MaybeAsASCII().c_str());
    provider->ResolvePpd(ref, base::Bind(&PpdProviderTest::CaptureResolvePpd,
                                         base::Unretained(this)));
    Drain(*provider);

    ASSERT_EQ(1UL, captured_resolve_ppd_.size());
    EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].first);
    EXPECT_EQ(user_ppd_contents, captured_resolve_ppd_[0].second);
  }
  // ScopedTempDir goes out of scope, so the source file should now be
  // deleted.  But if we resolve again, we should hit the cache and
  // still be successful.

  captured_resolve_ppd_.clear();

  // Recreate the provider to make sure we don't have any memory caches which
  // would mask problems with disk persistence.
  provider = CreateProvider("en");

  // Re-resolve.
  provider->ResolvePpd(ref, base::Bind(&PpdProviderTest::CaptureResolvePpd,
                                       base::Unretained(this)));
  Drain(*provider);

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].first);
  EXPECT_EQ(user_ppd_contents, captured_resolve_ppd_[0].second);
}

}  // namespace
}  // namespace printing
}  // namespace chromeos
