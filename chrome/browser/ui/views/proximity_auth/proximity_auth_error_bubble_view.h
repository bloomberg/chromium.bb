// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROXIMITY_AUTH_PROXIMITY_AUTH_ERROR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROXIMITY_AUTH_PROXIMITY_AUTH_ERROR_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/styled_label_listener.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace gfx {
class Range;
class Rect;
}

class ProximityAuthErrorBubbleView : public content::WebContentsObserver,
                                     public views::BubbleDelegateView,
                                     public views::StyledLabelListener {
 public:
  // Shows an error bubble with the given |message|, with an arrow pointing to
  // the |anchor_rect|. If the |link_range| is non-empty, then that range of the
  // |message| is drawn as a link with |link_url| as the target. When the link
  // is clicked, the target URL opens in a new tab off of the given
  // |web_contents|. Returns a weak pointer to the created bubble.
  static base::WeakPtr<ProximityAuthErrorBubbleView> Create(
      const base::string16& message,
      const gfx::Range& link_range,
      const GURL& link_url,
      const gfx::Rect& anchor_rect,
      content::WebContents* web_contents);

  const base::string16& message() { return message_; }

 private:
  ProximityAuthErrorBubbleView(const base::string16& message,
                               const gfx::Range& link_range,
                               const GURL& link_url,
                               const gfx::Rect& anchor_rect,
                               content::WebContents* web_contents);
  ~ProximityAuthErrorBubbleView() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(const gfx::Range& range,
                              int event_flags) override;

  // The message text shown in the bubble.
  base::string16 message_;

  // The target URL of the link shown in the bubble's error message. Ignored if
  // there is no link.
  const GURL link_url_;

  // Vends weak pointers to |this| instance.
  base::WeakPtrFactory<ProximityAuthErrorBubbleView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthErrorBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROXIMITY_AUTH_PROXIMITY_AUTH_ERROR_BUBBLE_VIEW_H_
