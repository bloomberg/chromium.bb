// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_TEST_MOCK_CLIENT_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_TEST_MOCK_CLIENT_H_

#include "base/macros.h"
#include "components/download/public/client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace download {
namespace test {

class MockClient : public Client {
 public:
  MockClient();
  ~MockClient() override;

  // Client implementation.
  MOCK_METHOD1(OnServiceInitialized, void(const std::vector<std::string>&));
  MOCK_METHOD3(
      OnDownloadStarted,
      ShouldDownload(const std::string&,
                     const std::vector<GURL>&,
                     const scoped_refptr<const net::HttpResponseHeaders>&));
  MOCK_METHOD2(OnDownloadUpdated, void(const std::string&, uint64_t));
  MOCK_METHOD2(OnDownloadFailed, void(const std::string&, FailureReason));
  MOCK_METHOD3(OnDownloadSucceeded,
               void(const std::string&, const base::FilePath&, uint64_t));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClient);
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_TEST_MOCK_CLIENT_H_
