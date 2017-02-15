// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_REQUIREMENTS_CHECKER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_REQUIREMENTS_CHECKER_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/requirements_checker.h"

namespace content {
class GpuFeatureChecker;
}

namespace extensions {
class Extension;

// Validates the 'requirements' extension manifest field. This is an
// asynchronous process that involves several threads, but the public interface
// of this class (including constructor and destructor) must only be used on
// the UI thread.
class ChromeRequirementsChecker : public RequirementsChecker {
 public:
  ChromeRequirementsChecker();
  ~ChromeRequirementsChecker() override;

 private:
  // RequirementsChecker:
  void Check(const scoped_refptr<const Extension>& extension,
             const RequirementsCheckedCallback& callback) override;

  // Callbacks for the GpuFeatureChecker.
  void SetWebGLAvailability(bool available);

  void MaybeRunCallback();

  std::vector<std::string> errors_;

  // Every requirement that needs to be resolved asynchronously will add to
  // this counter. When the counter is depleted, the callback will be run.
  int pending_requirement_checks_;

  scoped_refptr<content::GpuFeatureChecker> webgl_checker_;

  RequirementsCheckedCallback callback_;

  base::WeakPtrFactory<ChromeRequirementsChecker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRequirementsChecker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_REQUIREMENTS_CHECKER_H_
