// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_REQUIREMENTS_CHECKER_H_
#define CHROME_BROWSER_EXTENSIONS_REQUIREMENTS_CHECKER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_service.h"

class GPUFeatureChecker;

namespace extensions {
class Extension;

// Validates the 'requirements' extension manifest field. This is an
// asynchronous process that involves several threads, but the public interface
// of this class (including constructor and destructor) must only be used on
// the UI thread.
class RequirementsChecker : public base::SupportsWeakPtr<RequirementsChecker> {
 public:
  RequirementsChecker();
  ~RequirementsChecker();

  // The vector passed to the callback are any localized errors describing
  // requirement violations. If this vector is non-empty, requirements checking
  // failed. This should only be called once. |callback| will always be invoked
  // asynchronously on the UI thread. |callback| will only be called once, and
  // will be reset after called.
  void Check(scoped_refptr<const Extension> extension,
      base::Callback<void(std::vector<std::string> requirement)> callback);

 private:
  // Callbacks for the GPUFeatureChecker.
  void SetWebGLAvailability(bool available);
  void SetCSS3DAvailability(bool available);

  void MaybeRunCallback();

  std::vector<std::string> errors_;

  // Every requirement that needs to be resolved asynchronously will add to
  // this counter. When the counter is depleted, the callback will be run.
  int pending_requirement_checks_;

  scoped_refptr<GPUFeatureChecker> webgl_checker_;
  scoped_refptr<GPUFeatureChecker> css3d_checker_;

  base::Callback<void(std::vector<std::string> requirement_errorss)> callback_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_REQUIREMENTS_CHECKER_H_
