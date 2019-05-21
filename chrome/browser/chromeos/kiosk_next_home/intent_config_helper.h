// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_INTENT_CONFIG_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_INTENT_CONFIG_HELPER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "url/gurl.h"

namespace chromeos {
namespace kiosk_next_home {

// Helper that parses JSON config and checks whether intents are allowed or not
// according to it.
class IntentConfigHelper {
 public:
  // Interface that provides the JSON configuration content.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual std::string GetJsonConfig() const = 0;
  };

  // Returns instance built with real Delegate implementation that reads JSON
  // config from a resource file.
  static std::unique_ptr<IntentConfigHelper> GetInstance();

  explicit IntentConfigHelper(std::unique_ptr<Delegate> delegate);

  virtual ~IntentConfigHelper();

  // Returns true if intent is allowed according to the JSON config file.
  // Returns false before the parsing is complete or if config is invalid.
  virtual bool IsIntentAllowed(const GURL& intent_uri) const;

 protected:
  // For testing only.
  IntentConfigHelper();

 private:
  // Attempts to parse |json_config| via Data Decoder Service.
  void ParseConfig(const std::string& json_config);

  // Receives parsed JSON config when ready.
  void ParseConfigDone(base::Value config);

  // Called if config could not be parsed.
  void ParseConfigFailed(const std::string& error);

  // Until the JSON config has been parsed and ready_ set to true, all calls to
  // IsIntentAllowed will return false.
  bool ready_ = false;

  // Set of hosts allowed to be launched.
  base::flat_set<std::string> allowed_hosts_;

  // Maps allowed custom schemes to required paths.
  base::flat_map<std::string, std::string> allowed_custom_schemes_;

  base::WeakPtrFactory<IntentConfigHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IntentConfigHelper);
};

}  // namespace kiosk_next_home
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_INTENT_CONFIG_HELPER_H_
