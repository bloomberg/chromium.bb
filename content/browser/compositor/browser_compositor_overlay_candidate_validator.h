// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_H_

#include "cc/output/overlay_candidate_validator.h"

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT BrowserCompositorOverlayCandidateValidator
    : public cc::OverlayCandidateValidator {
 public:
  BrowserCompositorOverlayCandidateValidator() {}
  ~BrowserCompositorOverlayCandidateValidator() override {}

  virtual void SetSoftwareMirrorMode(bool enabled) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOverlayCandidateValidator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_H_
