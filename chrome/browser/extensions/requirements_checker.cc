// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/requirements_checker.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/gpu_feature_checker.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/gpu_feature_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

RequirementsChecker::RequirementsChecker()
    : pending_requirement_checks_(0) {
}

RequirementsChecker::~RequirementsChecker() {
}

void RequirementsChecker::Check(scoped_refptr<const Extension> extension,
    base::Callback<void(std::vector<std::string> errors)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  callback_ = callback;
  const Extension::Requirements& requirements = extension->requirements();

  if (requirements.npapi) {
#if defined(OS_CHROMEOS)
    errors_.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_NPAPI_NOT_SUPPORTED));
#endif  // defined(OS_CHROMEOS)
  }

  if (requirements.webgl) {
    ++pending_requirement_checks_;
    webgl_checker_ = new GPUFeatureChecker(
      content::GPU_FEATURE_TYPE_WEBGL,
      base::Bind(&RequirementsChecker::SetWebGLAvailability,
                 AsWeakPtr()));
  }

  if (requirements.css3d) {
    ++pending_requirement_checks_;
    css3d_checker_ = new GPUFeatureChecker(
      content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
      base::Bind(&RequirementsChecker::SetCSS3DAvailability,
                 AsWeakPtr()));
  }

  if (pending_requirement_checks_ == 0) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(callback_, errors_));
    // Reset the callback so any ref-counted bound parameters will get released.
    callback_.Reset();
    return;
  }
  // Running the GPU checkers down here removes any race condition that arises
  // from the use of pending_requirement_checks_.
  if (webgl_checker_.get())
    webgl_checker_->CheckGPUFeatureAvailability();
  if (css3d_checker_.get())
    css3d_checker_->CheckGPUFeatureAvailability();
}

void RequirementsChecker::SetWebGLAvailability(bool available) {
  if (!available) {
    errors_.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_WEBGL_NOT_SUPPORTED));
  }
  MaybeRunCallback();
}

void RequirementsChecker::SetCSS3DAvailability(bool available) {
  if (!available) {
    errors_.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_CSS3D_NOT_SUPPORTED));
  }
  MaybeRunCallback();
}

void RequirementsChecker::MaybeRunCallback() {
  if (--pending_requirement_checks_ == 0) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(callback_, errors_));
    // Reset the callback so any ref-counted bound parameters will get released.
    callback_.Reset();
    errors_.clear();
  }
}

}  // namespace extensions
