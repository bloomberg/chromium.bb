// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_installed_scripts_manager_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowserSideSender {
 public:
  explicit BrowserSideSender(std::vector<GURL> installed_urls)
      : installed_urls_(std::move(installed_urls)) {}

  mojom::ServiceWorkerInstalledScriptsInfoPtr CreateAndBind() {
    EXPECT_FALSE(manager_.is_bound());
    EXPECT_FALSE(body_handle_.is_valid());
    EXPECT_FALSE(meta_data_handle_.is_valid());
    auto scripts_info = mojom::ServiceWorkerInstalledScriptsInfo::New();
    scripts_info->installed_urls = installed_urls_;
    scripts_info->manager_request = mojo::MakeRequest(&manager_);
    return scripts_info;
  }

  const GURL& TransferInstalledScript() {
    EXPECT_FALSE(body_handle_.is_valid());
    EXPECT_FALSE(meta_data_handle_.is_valid());
    auto script_info = mojom::ServiceWorkerScriptInfo::New();
    const GURL& transferring_url = installed_urls()[next_transfer_index_];
    script_info->script_url = transferring_url;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, &body_handle_, &script_info->body));
    EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &meta_data_handle_,
                                                   &script_info->meta_data));
    manager_->TransferInstalledScript(std::move(script_info));
    next_transfer_index_++;
    return transferring_url;
  }

  void PushBody(const std::string& data) {
    PushDataPipe(data, body_handle_.get());
  }

  void PushMetaData(const std::string& data) {
    PushDataPipe(data, meta_data_handle_.get());
  }

  void FinishTransferBody() { body_handle_.reset(); }

  void FinishTransferMetaData() { meta_data_handle_.reset(); }

  const std::vector<GURL>& installed_urls() const { return installed_urls_; }

 private:
  void PushDataPipe(const std::string& data,
                    const mojo::DataPipeProducerHandle& handle) {
    ASSERT_TRUE(handle.is_valid());
    // Send |data| with null terminator.
    uint32_t written_bytes = data.size() + 1;
    MojoResult rv = mojo::WriteDataRaw(handle, data.c_str(), &written_bytes,
                                       MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    ASSERT_EQ(data.size() + 1, written_bytes);
  }

  const std::vector<GURL> installed_urls_;
  size_t next_transfer_index_ = 0;

  mojom::ServiceWorkerInstalledScriptsManagerPtr manager_;

  mojo::ScopedDataPipeProducerHandle body_handle_;
  mojo::ScopedDataPipeProducerHandle meta_data_handle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSideSender);
};

class WebServiceWorkerInstalledScriptsManagerImplTest : public testing::Test {
 public:
  WebServiceWorkerInstalledScriptsManagerImplTest()
      : io_thread_("io thread"),
        worker_thread_("worker thread"),
        worker_waiter_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {}

 protected:
  using RawScriptData =
      blink::WebServiceWorkerInstalledScriptsManager::RawScriptData;

  void SetUp() override {
    ASSERT_TRUE(io_thread_.Start());
    ASSERT_TRUE(worker_thread_.Start());
    io_task_runner_ = io_thread_.task_runner();
    worker_task_runner_ = worker_thread_.task_runner();
  }

  void TearDown() override {
    io_thread_.Stop();
    worker_thread_.Stop();
  }

  void CreateInstalledScriptsManager(
      mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info) {
    installed_scripts_manager_ =
        WebServiceWorkerInstalledScriptsManagerImpl::Create(
            std::move(installed_scripts_info), io_task_runner_);
  }

  base::WaitableEvent* IsScriptInstalledOnWorkerThread(const GURL& script_url,
                                                       bool* out_installed) {
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            [](blink::WebServiceWorkerInstalledScriptsManager*
                   installed_scripts_manager,
               const blink::WebURL& script_url, bool* out_installed,
               base::WaitableEvent* waiter) {
              *out_installed =
                  installed_scripts_manager->IsScriptInstalled(script_url);
              waiter->Signal();
            },
            installed_scripts_manager_.get(), script_url, out_installed,
            &worker_waiter_));
    return &worker_waiter_;
  }

  base::WaitableEvent* GetRawScriptDataOnWorkerThread(
      const GURL& script_url,
      std::unique_ptr<RawScriptData>* out_data) {
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            [](blink::WebServiceWorkerInstalledScriptsManager*
                   installed_scripts_manager,
               const blink::WebURL& script_url,
               std::unique_ptr<RawScriptData>* out_data,
               base::WaitableEvent* waiter) {
              *out_data =
                  installed_scripts_manager->GetRawScriptData(script_url);
              waiter->Signal();
            },
            installed_scripts_manager_.get(), script_url, out_data,
            &worker_waiter_));
    return &worker_waiter_;
  }

 private:
  // Provides SingleThreadTaskRunner for this test.
  const base::MessageLoop message_loop_;

  base::Thread io_thread_;
  base::Thread worker_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  base::WaitableEvent worker_waiter_;

  std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManager>
      installed_scripts_manager_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerInstalledScriptsManagerImplTest);
};

TEST_F(WebServiceWorkerInstalledScriptsManagerImplTest, GetRawScriptData) {
  const GURL kScriptUrl = GURL("https://example.com/installed1.js");
  const GURL kUnknownScriptUrl = GURL("https://example.com/not_installed.js");

  BrowserSideSender sender({kScriptUrl});
  CreateInstalledScriptsManager(sender.CreateAndBind());

  {
    bool result = false;
    IsScriptInstalledOnWorkerThread(kScriptUrl, &result)->Wait();
    // IsScriptInstalled returns correct answer even before script transfer
    // hasn't been started yet.
    EXPECT_TRUE(result);
  }

  {
    bool result = true;
    IsScriptInstalledOnWorkerThread(kUnknownScriptUrl, &result)->Wait();
    // IsScriptInstalled returns correct answer even before script transfer
    // hasn't been started yet.
    EXPECT_FALSE(result);
  }

  {
    std::unique_ptr<RawScriptData> script_data;
    const std::string kExpectedBody = "This is a script body.";
    const std::string kExpectedMetaData = "This is a meta data.";
    base::WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // Start transferring the script.
    EXPECT_EQ(kScriptUrl, sender.TransferInstalledScript());
    sender.PushBody(kExpectedBody);
    sender.PushMetaData(kExpectedMetaData);
    // GetRawScriptData should be blocked until body and meta data transfer are
    // finished.
    EXPECT_FALSE(get_raw_script_data_waiter->IsSignaled());
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();

    // Wait for the script's arrival.
    get_raw_script_data_waiter->Wait();
    ASSERT_TRUE(script_data);
    ASSERT_EQ(1u, script_data->ScriptTextChunks().size());
    ASSERT_EQ(kExpectedBody.size() + 1,
              script_data->ScriptTextChunks()[0].size());
    EXPECT_STREQ(kExpectedBody.data(),
                 script_data->ScriptTextChunks()[0].Data());
    ASSERT_EQ(1u, script_data->MetaDataChunks().size());
    ASSERT_EQ(kExpectedMetaData.size() + 1,
              script_data->MetaDataChunks()[0].size());
    EXPECT_STREQ(kExpectedMetaData.data(),
                 script_data->MetaDataChunks()[0].Data());
  }

  {
    std::unique_ptr<RawScriptData> script_data;
    GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data)->Wait();
    // This should not be blocked because the script has already been received.
    // nullptr will be served after the data has already been taken.
    EXPECT_EQ(nullptr, script_data.get());
  }
}

}  // namespace content
