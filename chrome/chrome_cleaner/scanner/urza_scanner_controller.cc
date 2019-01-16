// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/urza_scanner_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"

namespace chrome_cleaner {

UrzaScannerController::UrzaScannerController(
    MatchingOptions* options,
    std::unique_ptr<SignatureMatcherAPI> signature_matcher,
    RegistryLogger* registry_logger,
    ShortcutParserAPI* shortcut_parser)
    : ScannerController(registry_logger, shortcut_parser),
      signature_matcher_(std::move(signature_matcher)),
      scanner_(*options, signature_matcher_.get(), registry_logger) {
  DCHECK(signature_matcher_);
}

UrzaScannerController::~UrzaScannerController() {
  CHECK(!base::RunLoop::IsRunningOnCurrentThread());
  // TODO(joenotcharles): Clean up RunUntilIdle usage in loops.
  while (!scanner_.IsCompletelyDone())
    base::RunLoop().RunUntilIdle();
}

void UrzaScannerController::StartScan() {
  found_pups_.clear();
  scanner_.Start(base::BindRepeating(&UrzaScannerController::FoundUwSCallback,
                                     base::Unretained(this)),
                 base::BindOnce(&UrzaScannerController::DoneScanning,
                                base::Unretained(this)));
}

void UrzaScannerController::FoundUwSCallback(UwSId pup_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // UrzaScannerImpl reports PUP only once when they are found, so the result
  // code is updated only when necessary.
  found_pups_.push_back(pup_id);
  UpdateScanResults(found_pups_);
}

}  // namespace chrome_cleaner
