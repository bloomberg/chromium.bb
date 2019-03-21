// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

// A fake version of the SendTabToSelf model that doesn't do anything and return
// null pointers. Needed to setup the SendTabToSelfClientService.
class FakeSendTabToSelfModel : public SendTabToSelfModel {
 public:
  FakeSendTabToSelfModel() = default;
  ~FakeSendTabToSelfModel() override = default;

  std::vector<std::string> GetAllGuids() const override { return {}; }
  void DeleteAllEntries() override {}
  const SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const override {
    return nullptr;
  }
  const SendTabToSelfEntry* AddEntry(const GURL& url,
                                     const std::string& title,
                                     base::Time navigation_time) override {
    return nullptr;
  }
  void DeleteEntry(const std::string& guid) override {}
  void DismissEntry(const std::string& guid) override {}
};

// A test ReceivingUiHandler that keeps track of the number of entries for which
// DisplayNewEntry was called.
class TestReceivingUiHandler : public ReceivingUiHandler {
 public:
  TestReceivingUiHandler() = default;
  ~TestReceivingUiHandler() override {}

  void DisplayNewEntry(const SendTabToSelfEntry* entry) override {
    ++number_displayed_entries_;
  }

  void DismissEntries(const std::vector<std::string>& guids) override {}

  size_t number_displayed_entries() const { return number_displayed_entries_; }

 private:
  size_t number_displayed_entries_ = 0;
};

// A test SendTabToSelfClientService that exposes the TestReceivingUiHandler.
class TestSendTabToSelfClientService : public SendTabToSelfClientService {
 public:
  explicit TestSendTabToSelfClientService(SendTabToSelfModel* model)
      : SendTabToSelfClientService(nullptr, model) {}

  ~TestSendTabToSelfClientService() override = default;

  void SetupHandlerRegistry(Profile* profile) override {}

  TestReceivingUiHandler* SetupTestHandler() {
    test_handlers_.clear();
    std::unique_ptr<TestReceivingUiHandler> handler =
        std::make_unique<TestReceivingUiHandler>();
    TestReceivingUiHandler* handler_ptr = handler.get();
    test_handlers_.push_back(std::move(handler));
    return handler_ptr;
  }

  const std::vector<std::unique_ptr<ReceivingUiHandler>>& GetHandlers()
      const override {
    return test_handlers_;
  }

 protected:
  std::vector<std::unique_ptr<ReceivingUiHandler>> test_handlers_;
};

// Tests that the UI handlers gets notified of each entries that were added
// remotely.
TEST(SendTabToSelfClientServiceTest, MultipleEntriesAdded) {
  // Set up the test objects.
  FakeSendTabToSelfModel fake_model_;
  TestSendTabToSelfClientService client_service(&fake_model_);
  TestReceivingUiHandler* test_handler = client_service.SetupTestHandler();

  // Create 2 entries and simulated that they were both added remotely.
  SendTabToSelfEntry entry1("a", GURL("http://www.example-a.com"), "a site",
                            base::Time(), base::Time(), "device a");
  SendTabToSelfEntry entry2("b", GURL("http://www.example-b.com"), "b site",
                            base::Time(), base::Time(), "device b");
  client_service.EntriesAddedRemotely({&entry1, &entry2});

  EXPECT_EQ(2u, test_handler->number_displayed_entries());
}

}  // namespace

}  // namespace send_tab_to_self
