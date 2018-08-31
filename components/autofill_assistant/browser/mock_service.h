// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SERVICE_H_

#include <string>

#include "components/autofill_assistant/browser/service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockService : public Service {
 public:
  MockService();
  ~MockService() override;

  void GetScriptsForUrl(const GURL& url, ResponseCallback callback) override {
    // Transforming callback into a references allows using RunOnceCallback on
    // the argument.
    OnGetScriptsForUrl(url, callback);
  }
  MOCK_METHOD2(OnGetScriptsForUrl,
               void(const GURL& url, ResponseCallback& callback));

  void GetActions(const std::string& script_path,
                  ResponseCallback callback) override {
    OnGetActions(script_path, callback);
  }
  MOCK_METHOD2(OnGetActions,
               void(const std::string& script_path,
                    ResponseCallback& callback));

  void GetNextActions(
      const std::string& previous_server_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback) override {
    OnGetNextActions(previous_server_payload, processed_actions, callback);
  }
  MOCK_METHOD3(OnGetNextActions,
               void(const std::string& previous_server_payload,
                    const std::vector<ProcessedActionProto>& processed_actions,
                    ResponseCallback& callback));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SERVICE_H_
