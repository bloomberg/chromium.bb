// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SCANNER_URZA_SCANNER_CONTROLLER_H_
#define CHROME_CHROME_CLEANER_SCANNER_URZA_SCANNER_CONTROLLER_H_

#include <vector>

#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/shortcut_parser_api.h"
#include "chrome/chrome_cleaner/scanner/scanner_controller.h"
#include "chrome/chrome_cleaner/scanner/signature_matcher_api.h"
#include "chrome/chrome_cleaner/scanner/urza_scanner_impl.h"
#include "chrome/chrome_cleaner/settings/matching_options.h"

namespace chrome_cleaner {

// The Urza (pre-ESET) implementation of the ScannerController.
class UrzaScannerController : public ScannerController {
 public:
  UrzaScannerController(MatchingOptions* options,
                        std::unique_ptr<SignatureMatcherAPI> signature_matcher,
                        RegistryLogger* registry_logger,
                        ShortcutParserAPI* shortcut_parser);
  ~UrzaScannerController() override;

 protected:
  void StartScan() override;

 private:
  void FoundUwSCallback(UwSId pup_id);

  std::unique_ptr<SignatureMatcherAPI> signature_matcher_;
  UrzaScannerImpl scanner_;
  std::vector<UwSId> found_pups_;

  DISALLOW_COPY_AND_ASSIGN(UrzaScannerController);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SCANNER_URZA_SCANNER_CONTROLLER_H_
