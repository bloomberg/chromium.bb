// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATIONS_H_
#define CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATIONS_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/security_style_explanation.h"

namespace content {

// SecurityStyleExplanations contains human-readable explanations for
// why a particular SecurityStyle was chosen. Each
// SecurityStyleExplanation is a single security property of a page (for
// example, an expired certificate, a valid certificate, or the presence
// of active mixed content). A single site may have multiple different
// explanations of "secure", "warning", and "broken" severity levels.
struct SecurityStyleExplanations {
  CONTENT_EXPORT SecurityStyleExplanations();
  CONTENT_EXPORT ~SecurityStyleExplanations();

  std::vector<SecurityStyleExplanation> secure_explanations;
  std::vector<SecurityStyleExplanation> warning_explanations;
  std::vector<SecurityStyleExplanation> broken_explanations;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATION_H_
