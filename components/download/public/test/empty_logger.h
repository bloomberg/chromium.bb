// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_TEST_EMPTY_LOGGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_TEST_EMPTY_LOGGER_H_

#include "components/download/public/logger.h"

namespace download {
namespace test {

// A Logger that does nothing.
class EmptyLogger : public Logger {
 public:
  EmptyLogger() = default;
  ~EmptyLogger() override = default;

  // Logger implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::Value GetServiceStatus() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyLogger);
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_TEST_EMPTY_LOGGER_H_
