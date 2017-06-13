// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_SINGLE_LOG_SOURCE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_SINGLE_LOG_SOURCE_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/chromeos/system_logs/single_log_source.h"

namespace extensions {

// Provides a way to override the creation of a new SingleLogSource during
// testing.
class SingleLogSourceFactory {
 public:
  using CreateCallback =
      base::Callback<std::unique_ptr<system_logs::SingleLogSource>(
          system_logs::SingleLogSource::SupportedSource)>;

  // Returns a SingleLogSource with source type of |type|. The caller must take
  // ownership of the returned object.
  static std::unique_ptr<system_logs::SingleLogSource> CreateSingleLogSource(
      system_logs::SingleLogSource::SupportedSource type);

  // Pass in a callback that gets executed instead of the default behavior of
  // CreateSingleLogSource. Does not take ownership of |callback|. When done
  // testing, call this function again with |callback|=nullptr to restore the
  // default behavior.
  static void SetForTesting(CreateCallback* callback);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_SINGLE_LOG_SOURCE_FACTORY_H_
