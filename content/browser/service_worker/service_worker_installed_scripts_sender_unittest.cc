// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/histogram_tester.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void WriteBodyToDiskCache(
    ServiceWorkerResponseWriter* writer,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body) {
  std::unique_ptr<net::HttpResponseInfo> info =
      base::MakeUnique<net::HttpResponseInfo>();
  info->request_time = base::Time::Now();
  info->response_time = base::Time::Now();
  info->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.0 200 OK\0\0");
  for (const auto& header : headers)
    info->headers->AddHeader(header.first + ": " + header.second);

  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(info.release());
  info_buffer->response_data_size = body.size();
  {
    net::TestCompletionCallback cb;
    writer->WriteInfo(info_buffer.get(), cb.callback());
    int rv = cb.WaitForResult();
    EXPECT_GE(rv, 0);
  }

  scoped_refptr<net::IOBuffer> body_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(body);
  {
    net::TestCompletionCallback cb;
    writer->WriteData(body_buffer.get(), body.size(), cb.callback());
    int rv = cb.WaitForResult();
    EXPECT_EQ(body.size(), static_cast<size_t>(rv));
  }
}

void WriteMetaDataToDiskCache(ServiceWorkerResponseMetadataWriter* writer,
                              const std::string& meta_data) {
  scoped_refptr<net::IOBuffer> meta_data_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(meta_data);
  base::RunLoop loop;
  writer->WriteMetadata(
      meta_data_buffer.get(), meta_data.size(),
      base::Bind(
          [](base::Closure closure, int expected, int result) {
            EXPECT_EQ(expected, result);
            closure.Run();
          },
          loop.QuitClosure(), meta_data.size()));
  loop.Run();
}

void ReadDataPipeInternal(mojo::DataPipeConsumerHandle handle,
                          std::string* result,
                          base::OnceClosure quit_closure) {
  while (true) {
    uint32_t num_bytes;
    const void* buffer = nullptr;
    MojoResult rv =
        handle.BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        std::move(quit_closure).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::BindOnce(&ReadDataPipeInternal, handle, result,
                           base::Passed(&quit_closure)));
        return;
      case MOJO_RESULT_OK:
        EXPECT_NE(nullptr, buffer);
        EXPECT_GT(num_bytes, 0u);
        uint32_t before_size = result->size();
        result->append(static_cast<const char*>(buffer), num_bytes);
        uint32_t read_size = result->size() - before_size;
        EXPECT_EQ(num_bytes, read_size);
        rv = handle.EndReadData(read_size);
        EXPECT_EQ(MOJO_RESULT_OK, rv);
        break;
    }
  }
  NOTREACHED();
  return;
}

std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle handle) {
  EXPECT_TRUE(handle.is_valid());
  std::string result;
  base::RunLoop loop;
  ReadDataPipeInternal(handle.get(), &result, loop.QuitClosure());
  loop.Run();
  return result;
}

}  // namespace

class ExpectedScriptInfo {
 public:
  ExpectedScriptInfo(
      int64_t resource_id,
      const GURL& script_url,
      const std::vector<std::pair<std::string, std::string>>& headers,
      const std::string& encoding,
      const std::string& body,
      const std::string& meta_data)
      : resource_id_(resource_id),
        script_url_(script_url),
        headers_(headers),
        encoding_(encoding),
        body_(body),
        meta_data_(meta_data) {}

  ServiceWorkerDatabase::ResourceRecord WriteToDiskCache(
      ServiceWorkerStorage* storage) const {
    auto body_writer = storage->CreateResponseWriter(resource_id_);
    WriteBodyToDiskCache(body_writer.get(), headers_, body_);
    auto metadata_writer = storage->CreateResponseMetadataWriter(resource_id_);
    WriteMetaDataToDiskCache(metadata_writer.get(), meta_data_);
    return ServiceWorkerDatabase::ResourceRecord(resource_id_, script_url_,
                                                 body_.size());
  }

  void CheckIfIdentical(
      const mojom::ServiceWorkerScriptInfoPtr& script_info) const {
    EXPECT_EQ(script_url_, script_info->script_url);
    EXPECT_EQ(encoding_, script_info->encoding);
    for (const auto& header : headers_) {
      EXPECT_TRUE(base::ContainsKey(script_info->headers, header.first));
      EXPECT_EQ(header.second, script_info->headers[header.first]);
      script_info->headers.erase(header.first);
    }
    EXPECT_EQ(0u, script_info->headers.size());
    EXPECT_TRUE(script_info->body.is_valid());
    std::string body = ReadDataPipe(std::move(script_info->body));
    EXPECT_EQ(body_, body);
    if (meta_data_.size() == 0) {
      EXPECT_FALSE(script_info->meta_data.is_valid());
      EXPECT_EQ(0u, script_info->meta_data_size);
    } else {
      EXPECT_TRUE(script_info->meta_data.is_valid());
      std::string meta_data = ReadDataPipe(std::move(script_info->meta_data));
      EXPECT_EQ(meta_data_, meta_data);
    }
  }

  const GURL& script_url() const { return script_url_; }

 private:
  const int64_t resource_id_;
  const GURL script_url_;
  const std::vector<std::pair<std::string, std::string>> headers_;
  const std::string encoding_;
  const std::string body_;
  const std::string meta_data_;
};

class MockServiceWorkerInstalledScriptsManager
    : public mojom::ServiceWorkerInstalledScriptsManager {
 public:
  MockServiceWorkerInstalledScriptsManager(
      std::vector<GURL> installed_urls,
      mojom::ServiceWorkerInstalledScriptsManagerRequest request)
      : binding_(this, std::move(request)),
        installed_urls_(std::move(installed_urls)),
        next_url_(installed_urls_.begin()) {}

  mojom::ServiceWorkerScriptInfoPtr WaitUntilTransferInstalledScript() {
    EXPECT_TRUE(incoming_script_info_.is_null());
    EXPECT_FALSE(transfer_installed_script_waiter_);
    base::RunLoop loop;
    transfer_installed_script_waiter_ = loop.QuitClosure();
    loop.Run();
    EXPECT_FALSE(incoming_script_info_.is_null());
    return std::move(incoming_script_info_);
  }

  void TransferInstalledScript(
      mojom::ServiceWorkerScriptInfoPtr script_info) override {
    EXPECT_TRUE(incoming_script_info_.is_null());
    EXPECT_TRUE(transfer_installed_script_waiter_);
    EXPECT_EQ(*next_url_, script_info->script_url);
    incoming_script_info_ = std::move(script_info);
    if (transfer_installed_script_waiter_)
      std::move(transfer_installed_script_waiter_).Run();
    ++next_url_;
  }

 private:
  mojo::Binding<mojom::ServiceWorkerInstalledScriptsManager> binding_;
  // This is in the order to be served.
  const std::vector<GURL> installed_urls_;
  std::vector<GURL>::const_iterator next_url_;

  base::OnceClosure transfer_installed_script_waiter_;
  mojom::ServiceWorkerScriptInfoPtr incoming_script_info_;

  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerInstalledScriptsManager);
};

class ServiceWorkerInstalledScriptsSenderTest : public testing::Test {
 public:
  ServiceWorkerInstalledScriptsSenderTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

 protected:
  void SetUp() override {
    helper_ = base::MakeUnique<EmbeddedWorkerTestHelper>(base::FilePath());

    context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();

    pattern_ = GURL("http://www.example.com/test/");
    registration_ = base::MakeRefCounted<ServiceWorkerRegistration>(
        ServiceWorkerRegistrationOptions(pattern_), 1L, context()->AsWeakPtr());
    version_ = base::MakeRefCounted<ServiceWorkerVersion>(
        registration_.get(),
        GURL("http://www.example.com/test/service_worker.js"),
        context()->storage()->NewVersionId(), context()->AsWeakPtr());
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerVersion* version() { return version_.get(); }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

  GURL pattern_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
};

using SenderFinishedReason =
    ServiceWorkerInstalledScriptsSender::FinishedReason;

TEST_F(ServiceWorkerInstalledScriptsSenderTest, SendScripts) {
  const GURL kMainScriptURL = version()->script_url();
  std::string long_body = "I'm the script body!";
  std::string long_meta_data = "I'm the meta data!";
  long_body.resize(1E6, '!');
  long_meta_data.resize(1E6, '!');
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "1000000"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        std::move(long_body),
        std::move(long_meta_data)}},
      {GURL("https://example.com/imported1"),
       {2,
        GURL("https://example.com/imported1"),
        {{"Content-Length", "22"},
         {"Content-Type", "text/javascript; charset=euc-jp"},
         {"TestHeader", "BlahBlah"}},
        "euc-jp",
        "I'm imported script 1!",
        "I'm the meta data for imported script 1!"}},
      {GURL("https://example.com/imported2"),
       {3,
        GURL("https://example.com/imported2"),
        {{"Content-Length", "0"},
         {"Content-Type", "text/javascript; charset=shift_jis"},
         {"TestHeader", "BlahBlah"}},
        "shift_jis",
        "",
        ""}},
  };

  {
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(info.second.WriteToDiskCache(context()->storage()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender = base::MakeUnique<ServiceWorkerInstalledScriptsSender>(
      version(), kMainScriptURL, context()->AsWeakPtr());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  {
    mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::ContainsKey(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_request.is_pending());
    renderer_manager =
        base::MakeUnique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->installed_urls),
            std::move(scripts_info->manager_request));
  }
  ASSERT_TRUE(renderer_manager);

  sender->Start();

  while (kExpectedScriptInfoMap.size() > 0) {
    EXPECT_FALSE(sender->IsFinished());
    EXPECT_EQ(SenderFinishedReason::kNotFinished, sender->finished_reason());
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_TRUE(
        base::ContainsKey(kExpectedScriptInfoMap, script_info->script_url));
    const auto& info = kExpectedScriptInfoMap.at(script_info->script_url);
    info.CheckIfIdentical(script_info);
    kExpectedScriptInfoMap.erase(script_info->script_url);
  }

  EXPECT_TRUE(sender->IsFinished());
  EXPECT_EQ(SenderFinishedReason::kSuccess, sender->finished_reason());
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, FailedToSendBody) {
  const GURL kMainScriptURL = version()->script_url();
  std::string long_body = "I'm the body";
  long_body.resize(1E6, '!');
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "1000000"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        std::move(long_body),
        "I'm the meta data!"}},
  };

  {
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(info.second.WriteToDiskCache(context()->storage()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender = base::MakeUnique<ServiceWorkerInstalledScriptsSender>(
      version(), kMainScriptURL, context()->AsWeakPtr());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  {
    mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::ContainsKey(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_request.is_pending());
    renderer_manager =
        base::MakeUnique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->installed_urls),
            std::move(scripts_info->manager_request));
  }
  ASSERT_TRUE(renderer_manager);

  sender->Start();
  EXPECT_FALSE(sender->IsFinished());
  EXPECT_EQ(SenderFinishedReason::kNotFinished, sender->finished_reason());

  {
    // Reset a data pipe during sending the body.
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_TRUE(
        base::ContainsKey(kExpectedScriptInfoMap, script_info->script_url));
    script_info->body.reset();
    kExpectedScriptInfoMap.erase(script_info->script_url);
    // Wait until the error is triggered on the sender side.
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(sender->IsFinished());
  EXPECT_EQ(SenderFinishedReason::kConnectionError, sender->finished_reason());
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, FailedToSendMetaData) {
  const GURL kMainScriptURL = version()->script_url();
  std::string long_meta_data = "I'm the meta data!";
  long_meta_data.resize(1E6, '!');
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "0"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "",
        std::move(long_meta_data)}},
  };

  {
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(info.second.WriteToDiskCache(context()->storage()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender = base::MakeUnique<ServiceWorkerInstalledScriptsSender>(
      version(), kMainScriptURL, context()->AsWeakPtr());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  {
    mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::ContainsKey(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_request.is_pending());
    renderer_manager =
        base::MakeUnique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->installed_urls),
            std::move(scripts_info->manager_request));
  }
  ASSERT_TRUE(renderer_manager);

  sender->Start();
  EXPECT_FALSE(sender->IsFinished());
  EXPECT_EQ(SenderFinishedReason::kNotFinished, sender->finished_reason());

  {
    // Reset a data pipe during sending the meta data.
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_TRUE(
        base::ContainsKey(kExpectedScriptInfoMap, script_info->script_url));
    script_info->meta_data.reset();
    kExpectedScriptInfoMap.erase(script_info->script_url);
    // Wait until the error is triggered on the sender side.
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(sender->IsFinished());
  EXPECT_EQ(SenderFinishedReason::kMetaDataSenderError,
            sender->finished_reason());
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, Histograms) {
  const GURL kMainScriptURL = version()->script_url();
  // Use script bodies small enough to be read by one
  // ServiceWorkerResponseReader::ReadData(). The number of
  // ServiceWorker.DiskCache.ReadResponseResult will be two per script (one is
  // reading the body and the other is saying EOD).
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "17"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "Small script body",
        "I'm the meta data!"}},
      {GURL("https://example.com/imported1"),
       {2,
        GURL("https://example.com/imported1"),
        {{"Content-Length", "21"},
         {"Content-Type", "text/javascript; charset=euc-jp"},
         {"TestHeader", "BlahBlah"}},
        "euc-jp",
        "Small imported script",
        "I'm the meta data for imported script 1!"}},
  };

  {
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(info.second.WriteToDiskCache(context()->storage()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender = base::MakeUnique<ServiceWorkerInstalledScriptsSender>(
      version(), kMainScriptURL, context()->AsWeakPtr());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  {
    mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::ContainsKey(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_request.is_pending());
    renderer_manager =
        base::MakeUnique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->installed_urls),
            std::move(scripts_info->manager_request));
  }
  ASSERT_TRUE(renderer_manager);

  base::HistogramTester histogram_tester;
  sender->Start();

  while (kExpectedScriptInfoMap.size() > 0) {
    EXPECT_FALSE(sender->IsFinished());
    EXPECT_EQ(SenderFinishedReason::kNotFinished, sender->finished_reason());
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_TRUE(
        base::ContainsKey(kExpectedScriptInfoMap, script_info->script_url));
    const auto& info = kExpectedScriptInfoMap.at(script_info->script_url);
    info.CheckIfIdentical(script_info);
    kExpectedScriptInfoMap.erase(script_info->script_url);
  }

  EXPECT_TRUE(sender->IsFinished());
  EXPECT_EQ(SenderFinishedReason::kSuccess, sender->finished_reason());

  // The histogram should be recorded when reading the script.
  // The count should be four: reading the response body of a main script and an
  // imported script, and the end of reading bodies for the two scripts.
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.DiskCache.ReadResponseResult",
      ServiceWorkerMetrics::ReadResponseResult::READ_OK, 4);
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, NoInstalledScript) {
  // Create a scripts sender for a version that has no installed scripts.
  const GURL kMainScriptURL = version()->script_url();
  auto sender = base::MakeUnique<ServiceWorkerInstalledScriptsSender>(
      version(), kMainScriptURL, context()->AsWeakPtr());
  EXPECT_FALSE(sender->CreateInfoAndBind());
  EXPECT_FALSE(sender->IsFinished());
  EXPECT_EQ(SenderFinishedReason::kNotFinished, sender->finished_reason());

  sender->Start();
  // Finish immediately because there are no installed scripts.
  EXPECT_TRUE(sender->IsFinished());
  EXPECT_EQ(SenderFinishedReason::kSuccess, sender->finished_reason());
}

}  // namespace content
