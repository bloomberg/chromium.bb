// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/requirements_checker.h"

#include "base/bind.h"
#include "chrome/browser/gpu/gpu_feature_checker.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/requirements_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif

namespace extensions {

RequirementsChecker::RequirementsChecker()
    : pending_requirement_checks_(0) {
}

RequirementsChecker::~RequirementsChecker() {
}

void RequirementsChecker::Check(scoped_refptr<const Extension> extension,
    base::Callback<void(std::vector<std::string> errors)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  callback_ = callback;
  const RequirementsInfo& requirements =
      RequirementsInfo::GetRequirements(extension.get());

  if (requirements.npapi) {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    errors_.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_NPAPI_NOT_SUPPORTED));
#endif
#if defined(OS_WIN)
    if (base::win::IsMetroProcess()) {
      errors_.push_back(
          l10n_util::GetStringUTF8(IDS_EXTENSION_NPAPI_NOT_SUPPORTED));
    }
#endif
  }

  if (requirements.window_shape) {
#if !defined(USE_AURA)
    errors_.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_WINDOW_SHAPE_NOT_SUPPORTED));
#endif
  }

  if (requirements.webgl) {
    ++pending_requirement_checks_;
    webgl_checker_ = new GPUFeatureChecker(
      gpu::GPU_FEATURE_TYPE_WEBGL,
      base::Bind(&RequirementsChecker::SetWebGLAvailability,
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
}

void RequirementsChecker::SetWebGLAvailability(bool available) {
  if (!available) {
    errors_.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_WEBGL_NOT_SUPPORTED));
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
