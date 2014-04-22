// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

ManagePasswordsBubble::ManagePasswordsBubble(content::WebContents* contents,
                                             DisplayReason reason)
    : model_(new ManagePasswordsBubbleModel(contents)) {
  model()->OnBubbleShown(reason);
}

ManagePasswordsBubble::~ManagePasswordsBubble() {
  model()->OnBubbleHidden();
}
