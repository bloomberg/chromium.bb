// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

class ManagePasswordsBubbleModel;

namespace content {
class WebContents;
}  // namespace content

// Base class for platform-specific password management bubble views. This class
// is responsible for creating and destroying the model associated with the view
// and providing a platform-agnostic interface to the bubble.
class ManagePasswordsBubble {
 public:
  enum DisplayReason { AUTOMATIC, USER_ACTION };

 protected:
  // Creates a ManagePasswordsBubble. |contents| is used only to create a
  // ManagePasswordsBubbleModel; this class neither takes ownership of the
  // object nor stores the pointer.
  ManagePasswordsBubble(content::WebContents* contents, DisplayReason reason);
  ~ManagePasswordsBubble();

  ManagePasswordsBubbleModel* model() { return model_.get(); }
  const ManagePasswordsBubbleModel* model() const { return model_.get(); }

 private:
  scoped_ptr<ManagePasswordsBubbleModel> model_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubble);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_H_
