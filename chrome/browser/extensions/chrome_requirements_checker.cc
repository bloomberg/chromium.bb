// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_requirements_checker.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_feature_checker.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/requirements_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

ChromeRequirementsChecker::ChromeRequirementsChecker()
    : pending_requirement_checks_(0), weak_ptr_factory_(this) {
}

ChromeRequirementsChecker::~ChromeRequirementsChecker() {
}

void ChromeRequirementsChecker::Check(
    const scoped_refptr<const Extension>& extension,
    const RequirementsCheckedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  callback_ = callback;
  const RequirementsInfo& requirements =
      RequirementsInfo::GetRequirements(extension.get());

  if (requirements.npapi) {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    errors_.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_NPAPI_NOT_SUPPORTED));
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
    webgl_checker_ = content::GpuFeatureChecker::Create(
        gpu::GPU_FEATURE_TYPE_WEBGL,
        base::Bind(&ChromeRequirementsChecker::SetWebGLAvailability,
                   weak_ptr_factory_.GetWeakPtr()));
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
    webgl_checker_->CheckGpuFeatureAvailability();
}

void ChromeRequirementsChecker::SetWebGLAvailability(bool available) {
  if (!available) {
    errors_.push_back(
        l10n_util::GetStringUTF8(IDS_EXTENSION_WEBGL_NOT_SUPPORTED));
  }
  MaybeRunCallback();
}

void ChromeRequirementsChecker::MaybeRunCallback() {
  if (--pending_requirement_checks_ == 0) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(callback_, errors_));
    // Reset the callback so any ref-counted bound parameters will get released.
    callback_.Reset();
    errors_.clear();
  }
}

}  // namespace extensions
