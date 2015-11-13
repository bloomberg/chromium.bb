// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace dom_distiller {

// This is an IPC helper for determining whether a page should be distilled.
class DistillabilityDriver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DistillabilityDriver> {
 public:
  ~DistillabilityDriver() override;

  void SetDelegate(const base::Callback<void(bool, bool)>& delegate);

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* rfh) override;
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  explicit DistillabilityDriver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<DistillabilityDriver>;

  void OnDistillability(bool distillable, bool is_last);

  // Removes the observer and clears the WebContents member.
  void CleanUp();

  base::Callback<void(bool, bool)> m_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DistillabilityDriver);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLIBILITY_DRIVER_H_
